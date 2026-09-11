# Japanese LM / older Phob 2 trigger compatibility

The reported symptom was physical L operating the vacuum and the R-pump
counter, with physical R doing nothing to the vacuum. It affects the retail
game input, not just the overlay. There is no L/R swap in PADStatus.

## Protocol evidence

GLMJ01 revision 0 initializes JUTGamePad with PAD spec 5 and analog wire mode
0. Mode 0 allocates four bits each to L/R/analog A/analog B; mode 3 uses full
L/R bytes, while retaining eight-bit C-stick axes and the same digital bits.

[Phob v0.29 RP2040 communication source](https://github.com/PhobGCC/PhobGCC-SW/blob/v0.29/PhobGCC/rp2040/src/joybus.cpp)
reads the poll's mode byte but does not use it to transform the response.
Its [report layout](https://github.com/PhobGCC/PhobGCC-SW/blob/v0.29/PhobGCC/rp2040/include/comms/gcReport.hpp)
always ends in full L and R bytes. Thus a mode-0 decoder treats physical L's
low nibble as R and physical R as analog A/B. This reproduces the reported
symptom. The user's exact firmware version is unknown.

[Current Phob communication source](https://github.com/PhobGCC/PhobGCC-SW/blob/main/PhobGCC/rp2040/src/joybus.cpp)
converts each requested mode, and the [v0.31 release notes](https://github.com/PhobGCC/PhobGCC-SW/releases/tag/v0.31)
explicitly include an LM trigger fix. No controller firmware update or
controller settings mutation is performed by this mod.

## Authenticated patch

Clean DOL SHA-1: `722005ea9c1eab54b114f814734d8f327e5614ee`.

| Address | Retail | Patched | Purpose |
| --- | --- | --- | --- |
| `801D2074` | `li r0,0` | `li r0,3` | JUT analog-mode global |
| `801D207C` | `li r3,0` | `li r3,3` | PADSetAnalogMode argument |

The loader authenticates both old words and the intervening store/calls at
`801D2078/2080/2084`. Existing PADSetAnalogMode and PADInit are untouched.
The sole direct retail call to PADSetAnalogMode is `801D2080`; it shifts the
mode argument by eight and stores the SDK mode word at `804A0A70`. JUT's
matching mode word is `804A206C`. Soft initialization takes the same path.

Mode-0 nibble decoding is at `801E5198`, mode-3 byte decoding at `801E5294`.
The post-PADRead PADClamp call remains at `801D20BC`: both triggers keep the
retail dead zone 30 and cap 180 before subtraction. Button masks, calibration,
sticks, rumble, and native controls are not remapped. Mode 3 improves trigger
resolution; its unused analog A/B bytes are zero. Digital A/B are unchanged.

## Native game routing

JUTGamePad reads the raw array at `80494778`, with 12-byte port stride. Our
hook captures port 1 before PADClamp and before menu neutralization. CButton
copies raw `+6/+7` into its `+E/+F` at `801D25C4`, eventually JUT `+26/+27`.
Native selector `8007F440` maps binding 5 to L and binding 6 to R. Player
controls update `8007EA2C` writes the vacuum analog value to controls `+20`,
and `8007EA8C` writes element analog to `+2C`. The existing 0.3.30 MEM1
gameplay capture has bindings 6 and 5 respectively at controls `+12C/+144`.

## Verification and hardware checklist

`scripts/test_lm_pad_mode.py` authenticates the clean instructions and both
patches, checks the sole setter call, reproduces the old mode mismatch, and
checks all 65,536 L/R byte pairs under mode 3. Native timing tests separately
prove only R's digital mask/analog byte starts the R-pump counter. These do
not replace a Wii test of the user's controller.

Enable Input display and R-pump, release both triggers, then test L alone
and R alone, both soft analog pulls and full clicks. Raw L should respond
only to L, raw R only to R; the digital masks are L=`0040`, R=`0020`. Only R
starts the popup and vacuum. L operates the loaded element. Also confirm
A/B/menu/stick operation and recheck after a soft reboot.
