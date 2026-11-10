/* USART2 to the TLSR8253, fully remapped onto PD8 (TX) and PD9 (RX).
   The stock init asks for 76800, but the SoC that is on the board now
   sends valid FF 09 frames at 115200. Every frame is answered. 0x05D0
   carries the Doom button mask. 0x0500, 0x050E and 0x0522 carry the gauge. */

#include "gd32.h"
#include "platform.h"

#define USART2_BASE 0x40004800u
#define AFIO_PCF0 REG32(0x40010004u)
#define NVIC_ISER1 REG32(0xE000E104u)

#define RX_CAP 256u

static volatile uint8_t rx[RX_CAP];
static volatile uint16_t rx_w, rx_r;
static volatile uint32_t rx_bytes;
static volatile uint8_t btn_mask;
static volatile uint32_t btn_ms;
static volatile uint8_t btn_edge;

static uint16_t last_cmd;
static uint32_t replies;

static uint8_t frame[160];
static unsigned frame_n;
static unsigned frame_need;

static int rx_pop(uint8_t *out)
{
    uint16_t r = rx_r;
    if (r == rx_w)
        return 0;
    *out = rx[r % RX_CAP];
    rx_r = (uint16_t)(r + 1u);
    return 1;
}

static volatile uint32_t last_rx_ms;

void USART2_IRQHandler(void)
{
    uint32_t stat = REG32(USART2_BASE);
    if (stat & (1u << 5)) {
        uint8_t b = (uint8_t)REG32(USART2_BASE + 4u);
        uint16_t w = rx_w;
        last_rx_ms = plat_ms();
        if ((uint16_t)(w - rx_r) < RX_CAP) {
            rx[w % RX_CAP] = b;
            rx_w = (uint16_t)(w + 1u);
        }
        rx_bytes++;
    } else if (stat & 0x0Fu) {
        (void)REG32(USART2_BASE + 4u);
    }
}

static void tx_byte(uint8_t b)
{
    while ((REG32(USART2_BASE) & (1u << 7)) == 0u)
        ;
    REG32(USART2_BASE + 4u) = b;
}

static uint8_t xor_all(const uint8_t *p, unsigned n)
{
    uint8_t x = 0;
    while (n--)
        x ^= *p++;
    return x;
}

static void tx_frame(const uint8_t *req, const uint8_t *body, unsigned n)
{
    uint8_t raw[160];
    unsigned len;
    unsigned i;
    /* Stock protocol_response copies the request header and only sets the
       ACK bit. The TLSR sends byte 5 as 0x00; forcing 0x01 makes it drop
       the reply, so the phone never sees a notify. */
    if (n + 10u > sizeof raw)
        return;
    len = n + 10u;
    raw[0] = 0xFF;
    raw[1] = 0x09;
    raw[2] = (uint8_t)len;
    raw[3] = (uint8_t)(len >> 8);
    raw[4] = req[4];
    raw[5] = req[5];
    raw[6] = req[6];
    raw[7] = (uint8_t)((req[7] & 0x77u) | 0x08u);
    raw[8] = req[8];
    for (i = 0; i < n; i++)
        raw[9u + i] = body[i];
    raw[len - 1u] = xor_all(raw, len - 1u);
    for (i = 0; i < len; i++)
        tx_byte(raw[i]);
    replies++;
}

static int tlv_u8(const uint8_t *body, unsigned n, uint8_t kind, uint8_t *value)
{
    unsigned i = 0;
    while (i + 2u <= n) {
        unsigned ln = body[i + 1u];
        if (i + 2u + ln > n)
            return 0;
        if (body[i] == kind && ln >= 1u) {
            *value = body[i + 2u];
            return 1;
        }
        i += 2u + ln;
    }
    return 0;
}

static unsigned status_body(uint8_t *dst)
{
    const struct gauge_reading *g = gauge_now();
    unsigned n = 0;
    uint8_t soc_i = 0;
    uint8_t soc_f = 0;
    int temp_f;
    /* Stock's 0x0500 body starts with status 0, TLV A1 = 0x31, then a
       10-byte A2 whose first byte is 4 and whose last two bytes are the
       state of charge. The phone reads those two. Unknown port fields
       stay out of the body. */
    dst[n++] = 0x00;
    dst[n++] = 0xA1;
    dst[n++] = 1;
    dst[n++] = 0x31;
    if (g->ok && g->soc100) {
        soc_i = (uint8_t)(g->soc100 / 100u);
        soc_f = (uint8_t)(g->soc100 % 100u);
    }
    dst[n++] = 0xA2;
    dst[n++] = 10;
    dst[n++] = 4;
    for (unsigned i = 0; i < 7u; i++)
        dst[n++] = 0;
    dst[n++] = soc_i;
    dst[n++] = soc_f;

    dst[n++] = 0xB3;
    dst[n++] = 3;
    dst[n++] = 0;
    if (g->ok) {
        int t = g->temp_c;
        if (t < 0)
            t = 0;
        if (t > 255)
            t = 255;
        temp_f = t * 9 / 5 + 32;
        dst[n++] = (uint8_t)t;
        dst[n++] = (uint8_t)temp_f;
    } else {
        dst[n++] = 0;
        dst[n++] = 0;
    }
    return n;
}

static void handle_frame(const uint8_t *raw, unsigned len)
{
    uint8_t hi, body_b;
    uint16_t cmd;
    const uint8_t *body;
    unsigned blen;
    uint8_t reply[48];
    unsigned rn;

    if (len < 10u || raw[0] != 0xFF || raw[1] != 0x09)
        return;
    if (((unsigned)raw[2] | ((unsigned)raw[3] << 8)) != len)
        return;
    if (xor_all(raw, len - 1u) != raw[len - 1u])
        return;
    hi = raw[7];
    cmd = (uint16_t)(((hi & 0x0Fu) << 8) | raw[8]);
    body = raw + 9;
    blen = len - 10u;
    last_cmd = cmd;
    dbg_str("ble ");
    dbg_hex(cmd, 4);
    if (hi & 0x40u)
        dbg_str(" enc");
    dbg_nl();

    if (cmd == 0x05D0u && (hi & 0x40u) == 0u && tlv_u8(body, blen, 0xA2, &body_b)) {
        btn_mask = body_b;
        btn_ms = plat_ms();
        if (body_b)
            btn_edge = 1;
    }
    if (cmd == 0x0500u || cmd == 0x050Eu || cmd == 0x0522u)
        rn = status_body(reply);
    else if (cmd == 0x05D0u) {
        /* Same ack prefix as status. Without A1 = 0x31 the TLSR
           drops the notify and the phone never sees the button ack. */
        reply[0] = 0x00;
        reply[1] = 0xA1;
        reply[2] = 1;
        reply[3] = 0x31;
        rn = 4;
    } else {
        reply[0] = 0x00;
        rn = 1;
    }
    tx_frame(raw, reply, rn);
}

void ble_init(void)
{
    uint32_t pcf = AFIO_PCF0;
    pcf = (pcf & ~(3u << 4)) | (3u << 4);
    AFIO_PCF0 = pcf;

    REG32(USART2_BASE + 0x0Cu) = 0;
    /* APB1 is 60 MHz. 60000000/115200 rounds to 521. */
    REG32(USART2_BASE + 0x08u) = 521u;
    REG32(USART2_BASE + 0x0Cu) = (1u << 13) | (1u << 5) | (1u << 3) | (1u << 2);
    NVIC_ISER1 = 1u << (39u - 32u);
    dbg_str("usart2 115200");
    dbg_nl();
}

/* The TLSR arms the ACK waiter only after its own UART send returns.
   A reply in that window matches nothing, the timer expires, and the
   phone gets no notify. Completed frames sit here until the line has
   been quiet, and only one of them goes out per turn. */
#define Q_CAP 4u

static volatile uint8_t poll_busy;
static uint8_t q_slot[Q_CAP][160];
static uint8_t q_len[Q_CAP];
static unsigned q_r, q_n;
static uint32_t last_tx_ms;

static void q_push(const uint8_t *src, unsigned n)
{
    unsigned slot, i;
    if (n == 0u || n > sizeof q_slot[0])
        return;
    if (q_n >= Q_CAP) {
        q_r = (q_r + 1u) % Q_CAP;
        q_n--;
    }
    slot = (q_r + q_n) % Q_CAP;
    for (i = 0; i < n; i++)
        q_slot[slot][i] = src[i];
    q_len[slot] = (uint8_t)n;
    q_n++;
}

static int q_take(uint8_t *dst, unsigned *n)
{
    unsigned i, ln;
    if (q_n == 0u)
        return 0;
    ln = q_len[q_r];
    for (i = 0; i < ln; i++)
        dst[i] = q_slot[q_r][i];
    *n = ln;
    q_r = (q_r + 1u) % Q_CAP;
    q_n--;
    return 1;
}

void ble_poll(void)
{
    uint32_t prim;
    uint8_t b;
    uint8_t send[160];
    unsigned budget = 96;
    unsigned send_n = 0;
    __asm volatile("mrs %0, primask" : "=r"(prim));
    __asm volatile("cpsid i");
    if (poll_busy) {
        __asm volatile("msr primask, %0" :: "r"(prim) : "memory");
        return;
    }
    poll_busy = 1;
    __asm volatile("msr primask, %0" :: "r"(prim) : "memory");
    while (budget && rx_pop(&b)) {
        budget--;
        if (frame_n == 0) {
            if (b != 0xFF)
                continue;
        } else if (frame_n == 1 && b != 0x09) {
            frame_n = 0;
            frame_need = 0;
            if (b != 0xFF)
                continue;
        }
        if (frame_n >= sizeof frame) {
            frame_n = 0;
            frame_need = 0;
            continue;
        }
        frame[frame_n++] = b;
        if (frame_n == 4u) {
            frame_need = (unsigned)frame[2] | ((unsigned)frame[3] << 8);
            if (frame_need < 10u || frame_need > sizeof frame) {
                frame_n = 0;
                frame_need = 0;
            }
        }
        if (frame_need && frame_n >= frame_need) {
            q_push(frame, frame_need);
            frame_n = 0;
            frame_need = 0;
        }
    }
    if (q_n &&
        (uint32_t)(plat_ms() - last_rx_ms) >= 20u &&
        (uint32_t)(plat_ms() - last_tx_ms) >= 5u &&
        q_take(send, &send_n))
        last_tx_ms = plat_ms();
    if (send_n)
        handle_frame(send, send_n);
    poll_busy = 0;
}

uint8_t ble_buttons(void)
{
    if ((uint32_t)(plat_ms() - btn_ms) > 200u)
        return 0;
    return btn_mask;
}

uint8_t ble_pressed(void)
{
    uint8_t seen = btn_edge;
    btn_edge = 0;
    return seen;
}

uint32_t ble_bytes(void) { return rx_bytes; }
uint16_t ble_last(void) { return last_cmd; }
uint32_t ble_replies(void) { return replies; }
