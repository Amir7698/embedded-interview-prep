/*
 * ANSWERS: 08_IoT_Protocols/01_mqtt_coap.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: MQTT QoS 0, 1, 2 — differences and when to use each.

A: QoS 0 — At most once (fire and forget):
   Publisher sends once. No ACK. Broker may or may not deliver to subscriber.
   Overhead: just the PUBLISH packet.
   Use for: high-frequency telemetry where loss is OK (1 Hz temperature),
   live dashboards where missing one point is acceptable.

   QoS 1 — At least once:
   Publisher sends PUBLISH. Broker ACKs with PUBACK.
   If no PUBACK received: publisher retransmits with DUP flag.
   Subscriber may receive duplicates (DUP flag is a hint, not guaranteed).
   Use for: sensor alarms, device status changes where every event must arrive
   but the consumer can handle duplicates (idempotent processing).

   QoS 2 — Exactly once:
   Four-way handshake: PUBLISH → PUBREC → PUBREL → PUBCOMP.
   Guarantees exactly one delivery. Highest overhead.
   Use for: payment events, actuator commands (open valve exactly once),
   counting events where duplicates corrupt the result.

Q2: MQTT topic wildcard — single-level (+) vs multi-level (#).

A: + (plus): matches exactly one topic level.
   Example: sensors/+/temperature matches:
     sensors/kitchen/temperature ✓
     sensors/bedroom/temperature ✓
     sensors/floor1/room2/temperature ✗ (two levels)

   # (hash): matches zero or more topic levels. MUST be last character.
   Example: sensors/# matches:
     sensors/kitchen/temperature ✓
     sensors/bedroom/humidity ✓
     sensors/ ✓ (zero levels after sensors/)
   sensors/#/temperature is INVALID (# must be last).

   Cannot combine: sensors/+/# is invalid per MQTT spec.
   $SYS/# is a common pattern to subscribe to broker status.

Q3: MQTT vs CoAP for constrained devices.

A: MQTT:
   - TCP-based → reliable delivery, ordered, connection overhead.
   - Pub/sub: broker decouples publishers and subscribers.
   - Suitable for: NB-IoT, LTE-M with stable connections.
   - Broker required: single point of failure. Needs persistent TCP connection.
   - Minimum packet: ~2 bytes (PINGREQ). Connect overhead: significant.

   CoAP:
   - UDP-based → low overhead, tolerates packet loss (confirmable mode adds ACK).
   - REST-like: GET/POST/PUT/DELETE like HTTP.
   - Suitable for: 6LoWPAN, Zigbee, Thread, lossy networks.
   - No broker: direct client-server. CoAP Observe for push notifications.
   - Minimum packet: 4 bytes fixed header.

   Rule: MQTT for cloud-connected devices with stable IP connectivity.
         CoAP for M2M within constrained mesh networks (802.15.4).

Q4: TLS on embedded — mbedTLS vs wolfSSL.

A: mbedTLS (formerly PolarSSL, now Mbed TLS by Arm):
   - Default for Mbed OS, ESP-IDF, Zephyr.
   - Modular config (mbedtls_config.h) — disable unused algorithms to reduce size.
   - Flash footprint: ~60-100 KB for TLS 1.2 with RSA + AES.
   - Good documentation and active community.
   - FIPS 140-2 validation: available in commercial version.

   wolfSSL:
   - Designed specifically for embedded (formerly CyaSSL).
   - FIPS 140-2 validated out-of-box (wolfCrypt FIPS).
   - Smaller default footprint (~20-100 KB depending on config).
   - More commercial-grade certifications.

   Both support: TLS 1.2/1.3, mTLS (mutual authentication), PSK mode.
   PSK (Pre-Shared Key): no certificate negotiation, lowest overhead,
   ideal for device-to-broker where both sides are provisioned.
   Use: port 8883 for MQTT over TLS (vs 1883 for plaintext).

Q5: MQTT PUBLISH packet binary format.

A: Fixed header (1 byte):
   Bits [7:4] = packet type (3 = PUBLISH)
   Bit  3     = DUP flag
   Bits [2:1] = QoS (0/1/2)
   Bit  0     = RETAIN
   First byte for QoS=0, non-retain: 0x30

   Remaining length (1-4 bytes, variable-length encoding):
   If length < 128: 1 byte with MSB=0.
   If length ≥ 128: MSB=1, lower 7 bits = low part, next byte = continuation.

   Variable header:
   - Topic length: 2 bytes big-endian
   - Topic string: N bytes UTF-8
   - Packet identifier: 2 bytes (QoS 1/2 only)

   Payload: remaining bytes = message payload.

Q6: What is the retain flag in MQTT?

A: When a PUBLISH message has RETAIN=1:
   The broker stores the LAST retained message for that topic.
   Any subscriber who subscribes to that topic (now or in the future)
   immediately receives the last retained message upon subscription.
   Use cases:
   - Device status: publish "online"/"offline" as retained. New subscribers
     immediately know device state without waiting for next publish.
   - Configuration: publish a config message retained; new device instances
     get current config immediately after connecting and subscribing.
   Clearing retained message: publish empty payload (len=0) with RETAIN=1.
   Only ONE retained message per topic. Each new retained publish replaces the previous.
*/

/* ============================================================
 * TASK 1 — MQTT topic validation
 * ============================================================ */

int mqtt_topic_valid(const char *topic, uint8_t is_subscription)
{
    if (!topic || *topic == '\0') return 0;

    uint8_t has_hash = 0;
    const char *p = topic;

    while (*p) {
        if (*p == '#') {
            /* '#' must be last character and preceded by '/' or be first */
            if (*(p + 1) != '\0') return 0;   /* # not last */
            if (p != topic && *(p-1) != '/') return 0;  /* e.g., "a#" invalid */
            has_hash = 1;
        } else if (*p == '+') {
            /* '+' must occupy entire level: preceded by '/' or start, followed by '/' or end */
            if (p != topic && *(p-1) != '/') return 0;
            if (*(p+1) != '\0' && *(p+1) != '/') return 0;
        }
        p++;
    }
    (void)has_hash;

    /* Wildcards only allowed in subscriptions, not in publish topics */
    if (!is_subscription) {
        for (p = topic; *p; p++)
            if (*p == '#' || *p == '+') return 0;
    }
    return 1;
}

/* ============================================================
 * TASK 2 — MQTT remaining length encoding
 * ============================================================ */

uint8_t mqtt_encode_remaining_length(uint32_t length, uint8_t *out)
{
    uint8_t n = 0;
    do {
        uint8_t encoded = (uint8_t)(length % 128u);
        length /= 128u;
        if (length > 0) encoded |= 0x80u;
        out[n++] = encoded;
    } while (length > 0);
    return n;
}

uint32_t mqtt_decode_remaining_length(const uint8_t *in, uint8_t *bytes_used)
{
    uint32_t value = 0;
    uint8_t  shift = 0;
    uint8_t  i = 0;
    do {
        if (i > 3) break;
        value |= (uint32_t)(in[i] & 0x7Fu) << shift;
        shift += 7;
    } while (in[i++] & 0x80u);
    *bytes_used = i;
    return value;
}

/* ============================================================
 * TASK 3 — MQTT PUBLISH packet builder
 * ============================================================ */

uint16_t mqtt_build_publish(const char *topic, const uint8_t *payload,
                            uint16_t payload_len, uint8_t qos, uint8_t retain,
                            uint8_t *out, uint16_t out_max)
{
    uint16_t topic_len = (uint16_t)strlen(topic);
    uint32_t remaining = 2u + topic_len + payload_len;
    if (qos > 0) remaining += 2u;  /* packet identifier */

    uint8_t rem_buf[4];
    uint8_t rem_bytes = mqtt_encode_remaining_length(remaining, rem_buf);

    uint16_t total = 1u + rem_bytes + (uint16_t)remaining;
    if (total > out_max) return 0;

    uint16_t i = 0;
    out[i++] = (uint8_t)(0x30u | (qos << 1) | retain);
    memcpy(&out[i], rem_buf, rem_bytes); i += rem_bytes;
    out[i++] = (uint8_t)(topic_len >> 8);
    out[i++] = (uint8_t)(topic_len & 0xFF);
    memcpy(&out[i], topic, topic_len); i += topic_len;
    if (qos > 0) { out[i++] = 0x00; out[i++] = 0x01; }  /* packet id = 1 */
    memcpy(&out[i], payload, payload_len); i += payload_len;
    return i;
}

/* ============================================================
 * TASK 4 — JSON telemetry builder
 * ============================================================ */

int build_telemetry_json(char *buf, uint16_t bufsz,
                         uint32_t device_id, float temperature,
                         float humidity, uint32_t timestamp)
{
    return snprintf(buf, bufsz,
        "{\"device_id\":%u,\"temperature\":%.2f,"
        "\"humidity\":%.2f,\"timestamp\":%u}",
        device_id, (double)temperature, (double)humidity, timestamp);
}

/* ============================================================
 * TASK 5 — JSON value extractor
 * ============================================================ */

float json_get_float(const char *json, const char *key)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0.0f;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    return strtof(p, NULL);
}

int json_get_int(const char *json, const char *key)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

/* ============================================================
 * TASK 6 — Bug hunt: invalid topics
 *
 * Bug 1: "#/temperature" — '#' is not last character.
 *        MQTT spec 4.7.1.2: when '#' is used, it MUST be the last character.
 *        This would cause brokers to reject the subscription.
 *
 * Bug 2: "" (empty string) — topic must not be empty per MQTT spec.
 *        Zero-length topic in PUBLISH is invalid. Brokers reject it.
 *
 * Bug 3: "+x" — '+' must occupy an entire topic level.
 *        The '+' must be the only character in its level (between '/' delimiters).
 *        "+x" is not a valid level wildcard.
 *
 * VALID examples:
 *   "sensors/+/temperature"  — valid, + matches one level
 *   "devices/#"              — valid, # matches all remaining levels
 *   "data"                   — valid, exact topic
 *   "a/b/c"                  — valid, three-level topic
 * ============================================================ */

int main(void)
{
    /* Topic validation */
    assert(mqtt_topic_valid("sensors/+/temperature", 1) == 1);   /* valid subscription */
    assert(mqtt_topic_valid("devices/#", 1) == 1);
    assert(mqtt_topic_valid("#/temperature", 1) == 0);   /* Bug 1: # not last */
    assert(mqtt_topic_valid("", 1) == 0);                /* Bug 2: empty */
    assert(mqtt_topic_valid("+x", 1) == 0);              /* Bug 3: + not full level */
    assert(mqtt_topic_valid("sensors/temp", 0) == 1);    /* valid publish topic */
    assert(mqtt_topic_valid("sensors/+", 0) == 0);       /* wildcard in publish = invalid */

    /* Remaining length encoding */
    uint8_t rem[4];
    uint8_t n = mqtt_encode_remaining_length(0, rem);
    assert(n == 1 && rem[0] == 0x00);
    n = mqtt_encode_remaining_length(127, rem);
    assert(n == 1 && rem[0] == 127);
    n = mqtt_encode_remaining_length(128, rem);
    assert(n == 2 && rem[0] == 0x80 && rem[1] == 0x01);
    n = mqtt_encode_remaining_length(16383, rem);
    assert(n == 2);

    uint8_t used;
    uint32_t val = mqtt_decode_remaining_length(rem, &used);
    assert(val == 16383 && used == 2);

    /* PUBLISH packet */
    uint8_t pkt[128];
    const char *topic = "sensor/temp";
    const uint8_t payload[] = {'2','5','.','5'};
    uint16_t plen = mqtt_build_publish(topic, payload, 4, 0, 0, pkt, sizeof(pkt));
    assert(plen > 0);
    assert(pkt[0] == 0x30);  /* PUBLISH, QoS=0, no retain */
    /* Topic len in bytes 2-3 */
    uint16_t topic_in_pkt = ((uint16_t)pkt[2] << 8) | pkt[3];
    assert(topic_in_pkt == (uint16_t)strlen(topic));

    /* JSON builder */
    char json[128];
    build_telemetry_json(json, sizeof(json), 42, 25.5f, 60.0f, 1000000);
    printf("JSON: %s\n", json);
    assert(strstr(json, "\"device_id\":42") != NULL);

    float t = json_get_float(json, "temperature");
    assert(t > 25.4f && t < 25.6f);

    printf("All MQTT/CoAP answers verified.\n");
    return 0;
}
