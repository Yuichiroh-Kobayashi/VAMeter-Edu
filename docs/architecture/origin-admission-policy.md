# Direct-browser Origin admission policy

This document is the canonical security policy for direct-browser WebSocket admission. G2 freezes `O1-RX + O3`, retains `C_HYBRID`, makes P1 and P2 mandatory, keeps O2 as a conditional fallback only, and reserves O4 as the last resort.

## Threat model and security boundary

Browser `Origin` metadata limits which browser origins may open the measurement WebSocket. It reduces cross-origin browser abuse; it is not authentication, authorization of a person, proof of device ownership, transport confidentiality, or protection from a non-browser client that can construct arbitrary HTTP.

The policy is fail closed. Ambiguous or unsupported syntax is rejected rather than normalized into an accepted request. Production policy must never be widened to accommodate external-development convenience.

P1 and P2 are mandatory:

- **P1:** reject an invalid stream admission before D2B owner mutation, `OpenSession`, or per-client pipeline allocation.
- **P2:** reject an invalid stream admission before ESP-IDF emits the WebSocket HTTP 101 response.

Connection close without an HTTP error body is an acceptable rejection result when it preserves P1 and P2.

## Authority

### Production

The expected production Origin is established from trusted device-side SystemLive network/service state before HTTPD admission begins. Incoming `Host` is not the security authority and must not widen admission. After SystemLive startup, the accepted production Origin is immutable for that lifecycle.

The current transport is HTTP on port 80. This contract does not promise hostname or mDNS aliases. A future transport, port, hostname, or alias requires a separately reviewed exact authority policy.

### External development

External development uses a development-only build/profile with an explicit immutable exact Origin allowlist. It permits:

- no wildcard;
- no reflected incoming value;
- no suffix or substring match;
- no implicit fallback to production policy.

Literal development Origin values are deployment/test inputs. They must be supplied and reviewed for the specific environment, not invented in architecture documentation.

## O1-RX integration

O1-RX uses the public ESP-IDF `open_fn` and per-session receive override before the HTTP parser sees request bytes. The VAMeter-owned open wrapper must:

1. chain the public `PsychicHttpServer::openCallback`;
2. allocate bounded scanner state;
3. install the public ESP-IDF session transport context and recv override;
4. preserve the existing PsychicHttp close callback and cleanup path.

The receive override calls public POSIX/lwIP `recv` directly, scans the returned bytes, and returns accepted bytes unchanged. It must not call `httpd_socket_recv()` from inside the override because that redispatches the override.

This architecture uses no private ESP-IDF or PsychicHttp API and requires no dependency source modification. Public-surface feasibility is pinned-version evidence, not a promise of future ABI compatibility.

## Strict accepted subset

O1-RX is a bounded admission scanner, not a replacement HTTP parser. It accepts only the unambiguous SystemLive browser subset needed to distinguish ordinary GETs from the exact stream upgrade:

- canonical HTTP/1.1 `GET` in origin-form with CRLF;
- exact stream target `/d2b/v0/stream`, without query, fragment, or absolute-form variants;
- no request body, `Content-Length`, `Transfer-Encoding`, obs-fold, or bare LF;
- canonical, unambiguous admission-critical fields.

For `/d2b/v0/stream` WebSocket admission, require exactly one valid Origin and the canonical unique upgrade semantics. Reject:

- missing Origin;
- literal `null`;
- empty, malformed, or overlong Origin;
- duplicate Origin fields;
- any comma-bearing Origin value;
- folded, ambiguously spaced, or otherwise non-canonical critical values;
- duplicate or ambiguous admission-critical upgrade fields.

Header names are ASCII case-insensitive. Origin value bytes are not normalized and must exactly match the selected immutable authority. Any parser differential or ambiguity fails closed.

The Origin requirement applies only to WebSocket admission at exact `/d2b/v0/stream`. Normal HTTP GET for the device-hosted Viewer, `device.json`, D2B capabilities, and D2B status does not require Origin. The scanner is active only for SystemLive, never as a policy overlay on SystemConfig or Download.

## Fragmentation, pipelining, and transition

Parsing state carries across arbitrary receive fragmentation, including one-byte boundaries through request line, fields, values, and CRLF.

After an accepted ordinary GET, the scanner must continue scanning all remaining bytes in the same receive block as the next request. This prevents a pipelined request from entering ESP-IDF pending storage without admission scanning. Because the accepted SystemLive subset is bodyless GET-only, body bytes cannot be confused with an accepted next request.

After an accepted WebSocket header, the scanner switches to permanent transparent receive pass-through at the exact header terminator. Any first-frame bytes following that terminator in the same block, and every later receive, pass through unchanged and are never interpreted as HTTP.

A terminal violation returns socket failure without returning the header-completing block to HTTPD. Closing the connection is preferable to ambiguous partial admission.

## Fixed bounds

- Maximum accepted Origin value: 191 bytes.
- Per-session O1-RX state design ceiling: 64 bytes, excluding allocator metadata.
- No full-header or replay buffer.

These are implementation ceilings, not measured resource cost. Actual IRAM, DRAM, BSS, flash, heap, and stack effects require later measurement under the
[resource budget](resource-budget.md) and the validation hierarchy in
[`../ai/build-and-validation.md`](../ai/build-and-validation.md).

Route and topology ownership is defined by
[`direct-browser-service-profiles.md`](direct-browser-service-profiles.md). No O1-RX implementation or qualification is established by this architecture freeze.
