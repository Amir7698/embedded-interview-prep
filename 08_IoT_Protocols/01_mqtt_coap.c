/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : IoT Protocols — MQTT, CoAP, JSON, TLS
 * File  : 08_IoT_Protocols/01_mqtt_coap.c
 * ============================================================
 *
 * MQTT is the dominant protocol for IoT.
 * CoAP is the embedded HTTP alternative for constrained devices.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — MQTT (Message Queuing Telemetry Transport)
 * ============================================================
 *
 * MQTT v3.1.1 / v5.0 — broker-based pub/sub over TCP
 *
 * Key concepts:
 *   Broker   : central server (Mosquitto, HiveMQ, AWS IoT)
 *   Publisher: sends messages to a topic
 *   Subscriber: receives messages from topics it subscribed to
 *   Topic    : hierarchical string, e.g. "factory/line1/sensor/temp"
 *              Wildcard '+' = single level: "factory/+/sensor/+"
 *              Wildcard '#' = multi level:  "factory/#"
 *
 * QoS levels:
 *   0 = At most once  (fire and forget — possible loss)
 *   1 = At least once (guaranteed delivery, possible duplicates)
 *   2 = Exactly once  (two-phase handshake, no duplicates)
 *
 * MQTT CONNECT packet fields:
 *   ClientID, CleanSession, KeepAlive (seconds)
 *   Will topic/message (sent by broker if client disconnects unexpectedly)
 *   Username/Password
 *
 * Keep-alive: client must send PINGREQ within KeepAlive interval.
 * Broker closes connection if no message/PINGREQ within 1.5×KeepAlive.
 *
 * MQTT over TLS: port 8883 (vs 1883 plaintext)
 * MQTT over WebSocket: port 443 (for browser clients)
 *
 * Fixed header byte 1:
 *   Bits [7:4] = message type (CONNECT=1, PUBLISH=3, SUBSCRIBE=8, PINGREQ=12)
 *   Bits [3:0] = flags (DUP, QoS, RETAIN for PUBLISH)
 *
 * PUBLISH packet structure:
 *   [Fixed Header][Variable Header: Topic + Packet ID (if QoS>0)][Payload]
 * ============================================================ */

/* ============================================================
 * THEORY — CoAP (Constrained Application Protocol)
 * ============================================================
 *
 * CoAP = "HTTP for constrained devices" — runs over UDP (not TCP)
 * RFC 7252
 *
 * Methods: GET, POST, PUT, DELETE (like HTTP)
 * Request/Response model with confirmable (reliable) or non-confirmable messages
 * Uses binary encoding (not text like HTTP)
 * Supports observe (subscribe to resource changes — like MQTT)
 *
 * CoAP vs MQTT:
 *   MQTT: broker-based, pub/sub, TCP, persistent connections
 *   CoAP: peer-to-peer (or proxy), request/response, UDP, stateless
 *
 * CoAP fixed header (4 bytes):
 *   [Ver:2][T:2][TKL:4][Code:8][Message ID:16]
 *   Ver = 01 (always), T = Confirmable/Non-Confirmable/Ack/Reset
 *   TKL = token length (0-8), Code = 2.05 = "Content"
 *
 * Use CoAP when:
 *   - Device is battery-powered, uses cellular/LoRa (UDP saves overhead)
 *   - Peer-to-peer control (no broker needed)
 *   - Very low memory (< 10 KB RAM for full CoAP stack)
 * ============================================================ */

/* ============================================================
 * TASK 1 — MQTT topic string validation
 * ============================================================ */

int mqtt_topic_valid(const char *topic)
{
    /* TODO: topic must not be empty
     * TODO: topic must not contain null in the middle
     * TODO: '+' may only appear as a complete level: "a/+/b" OK, "a/+x/b" NOT OK
     * TODO: '#' may only appear at the very end, as a complete level: "a/b/#" OK
     *        "a/#/b" NOT OK, "a/b#" NOT OK
     * Return 1 if valid, 0 if not */
    if (!topic || !topic[0]) return 0;
    size_t len = strlen(topic);
    for (size_t i = 0; i < len; i++) {
        char c = topic[i];
        if (c == '+') {
            if ((i > 0 && topic[i-1] != '/') || (topic[i+1] != '/' && topic[i+1] != '\0'))
                return 0;
        }
        if (c == '#') {
            if ((i > 0 && topic[i-1] != '/') || topic[i+1] != '\0')
                return 0;
        }
    }
    return 1;
}

/* ============================================================
 * TASK 2 — MQTT PUBLISH packet builder (QoS 0)
 *
 * PUBLISH fixed header byte 1: 0x30 (type=PUBLISH, QoS=0, no retain, no dup)
 * Remaining length: variable-length encoding
 * Variable header: topic length (2 bytes BE) + topic string
 * Payload: the message bytes
 * ============================================================ */

int mqtt_encode_remaining_length(uint32_t value, uint8_t *out)
{
    /* MQTT uses variable-length encoding: 7 bits per byte, bit7=continuation flag */
    int len = 0;
    do {
        uint8_t encoded = value & 0x7Fu;
        value >>= 7;
        if (value > 0) encoded |= 0x80u;
        out[len++] = encoded;
    } while (value > 0 && len < 4);
    return len;
}

int mqtt_build_publish(const char *topic, const uint8_t *payload, uint16_t payload_len,
                       uint8_t *out, uint16_t out_max)
{
    /* TODO: out[0] = 0x30 (PUBLISH, QoS=0, retain=0, dup=0)
     * TODO: variable header = 2 + topic_len + payload_len
     * TODO: encode remaining length into out[1..]
     * TODO: write topic_len big-endian
     * TODO: write topic string
     * TODO: write payload
     * Return total length or -1 */
    uint16_t topic_len = (uint16_t)strlen(topic);
    uint32_t remaining = 2 + topic_len + payload_len;

    uint8_t rem_enc[4];
    int rem_len = mqtt_encode_remaining_length(remaining, rem_enc);

    uint32_t total = 1 + rem_len + remaining;
    if (total > out_max) return -1;

    int pos = 0;
    out[pos++] = 0x30u;
    for (int i = 0; i < rem_len; i++) out[pos++] = rem_enc[i];
    out[pos++] = (uint8_t)(topic_len >> 8);
    out[pos++] = (uint8_t)(topic_len & 0xFFu);
    memcpy(out + pos, topic, topic_len); pos += topic_len;
    memcpy(out + pos, payload, payload_len); pos += payload_len;
    return pos;
}

/* ============================================================
 * TASK 3 — JSON payload builder for IoT telemetry
 *
 * Produce: {"device":"sensor01","temp":23.5,"hum":65,"ts":1720000000}
 * Without using JSON library — manual string construction.
 * ============================================================ */

int build_telemetry_json(const char *device_id, float temp, float humidity,
                          uint32_t timestamp, char *out, uint16_t out_max)
{
    /* TODO: use snprintf to build the JSON string.
     * Return length (excluding null), or -1 if truncated. */
    int n = snprintf(out, out_max,
        "{\"device\":\"%s\",\"temp\":%.1f,\"hum\":%.0f,\"ts\":%lu}",
        device_id, (double)temp, (double)humidity, (unsigned long)timestamp);
    if (n < 0 || n >= out_max) return -1;
    return n;
}

/* ============================================================
 * TASK 4 — Parse simple JSON key-value (no library)
 *
 * Extract a float value from: {"key": value, ...}
 * Simple string search approach (not robust, but common in embedded).
 * ============================================================ */

int json_get_float(const char *json, const char *key, float *out)
{
    /* TODO: find "\"key\":" in json string
     * skip whitespace after ':'
     * parse float using strtof
     * return 0 on success, -1 if key not found */
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *pos = strstr(json, search);
    if (!pos) return -1;
    pos += strlen(search);
    while (*pos == ' ') pos++;
    char *end;
    *out = strtof(pos, &end);
    return (end != pos) ? 0 : -1;
}

/* ============================================================
 * TASK 5 — TLS concepts quiz (no code — describe the handshake)
 *
 * TLS 1.2 handshake (simplified):
 * 1. Client Hello: supported cipher suites, random nonce
 * 2. Server Hello: chosen cipher suite, server random nonce, certificate
 * 3. Client verifies certificate (CA chain, hostname, not expired)
 * 4. Key exchange: Client generates pre-master secret, encrypts with server's
 *    public key (RSA) or uses Diffie-Hellman (ECDHE)
 * 5. Both sides derive session keys (symmetric)
 * 6. Handshake complete: data encrypted with AES-128/256
 *
 * For embedded devices:
 *   mbedTLS / wolfSSL: lightweight TLS stacks
 *   Minimum RAM: ~50 KB for TLS handshake buffer
 *   Certificate storage: in flash (DER format, smaller than PEM)
 *   Mutual TLS (mTLS): device also presents a certificate (device authentication)
 *   Pre-Shared Key (PSK): no certificates, just a shared secret (simpler, for IoT)
 * ============================================================ */

void tls_concepts_quiz(void)
{
    printf("TLS for embedded:\n");
    printf("  Use mbedTLS or wolfSSL (wolfSSL is smaller: ~50KB flash)\n");
    printf("  mTLS: both client and server authenticate with certificates\n");
    printf("  PSK: no certs needed — simpler for closed IoT systems\n");
    printf("  Port 8883: MQTT over TLS\n");
    printf("  Min heap for TLS handshake: ~50KB\n");
}

/* ============================================================
 * TASK 6 — BUG HUNT: MQTT topic usage bugs
 *
 * The code below has 3 MQTT-related bugs.
 * Find and mark each one.
 * ============================================================ */

void mqtt_usage_BUGGY(void)
{
    const char *topics[] = {
        /* Bug 1: invalid wildcard placement — '#' must be at the end and full level */
        "factory/#/temp",   /* INVALID */

        /* Bug 2: empty topic — not allowed */
        "",                 /* INVALID */

        /* Bug 3: '+' is not a full level */
        "device/+x/data",  /* INVALID — '+x' is not just '+' */
    };

    for (int i = 0; i < 3; i++) {
        printf("Topic '%s': %s\n", topics[i],
               mqtt_topic_valid(topics[i]) ? "valid" : "INVALID (bug)");
    }
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    /* Topic validation */
    assert(mqtt_topic_valid("sensor/temp") == 1);
    assert(mqtt_topic_valid("factory/+/data") == 1);
    assert(mqtt_topic_valid("factory/#") == 1);
    assert(mqtt_topic_valid("factory/#/temp") == 0);
    assert(mqtt_topic_valid("") == 0);

    /* JSON builder */
    char json_buf[128];
    int n = build_telemetry_json("dev01", 23.5f, 65.0f, 1720000000u, json_buf, sizeof(json_buf));
    assert(n > 0);
    printf("JSON: %s\n", json_buf);

    /* JSON parser */
    float temp;
    assert(json_get_float(json_buf, "temp", &temp) == 0);
    printf("Parsed temp: %.1f\n", (double)temp);

    /* MQTT packet */
    uint8_t packet[128];
    int plen = mqtt_build_publish("sensor/temp", (uint8_t*)"23.5", 4, packet, sizeof(packet));
    assert(plen > 0);
    assert(packet[0] == 0x30);

    tls_concepts_quiz();
    mqtt_usage_BUGGY();

    printf("\nAll IoT protocol tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: What is the difference between MQTT QoS 0, 1, and 2?
 *     When would you use each on an embedded device?
 *     Answer: TODO
 *
 * Q2: An IoT device has intermittent connectivity. How does MQTT's
 *     "Last Will and Testament" help? What is the message?
 *     Answer: TODO
 *
 * Q3: Compare MQTT and CoAP for a battery-powered sensor that reports
 *     temperature every 30 minutes over a cellular modem.
 *     Answer: TODO
 *
 * Q4: What is TLS mutual authentication (mTLS)?
 *     Why is it important for IoT devices?
 *     Answer: TODO
 *
 * Q5: Your embedded MQTT client connects to AWS IoT.
 *     What certificates do you need and where do you store them?
 *     Answer: TODO
 */
