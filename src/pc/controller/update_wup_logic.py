import math
import textwrap
import sympy as sym
from collections import namedtuple

# configDeadZoneFrac = 4960 / (2 ** 15)

PayloadButon = namedtuple('PayloadButon', ['index', 'bitv'])
AxisPoints = namedtuple('PayloadButon', ['index', 'left_extreme', 'left_mid', 'left_zero', 'right_zero', 'right_mid', 'right_extreme'])

# Mapping from button names to the payload index and associated bit value.
payload_buttons = {}
payload_buttons['Z'] = PayloadButon(0, 64)
payload_buttons['R'] = PayloadButon(0, 32)
payload_buttons['L'] = PayloadButon(0, 16)
payload_buttons['A'] = PayloadButon(0, 4)
payload_buttons['B'] = PayloadButon(0, 2)
payload_buttons['START'] = PayloadButon(1, 2)

# Mapping from button names to target bits to their expected bits in port->buttons
adapter_buttons = {}
adapter_buttons['START'] = 0x0001
adapter_buttons['Z'] = 0x0008
adapter_buttons['R'] = 0x0004
adapter_buttons['A'] = 0x0100
adapter_buttons['B'] = 0x0200
adapter_buttons['L'] = 0x1000

# Mapping from axis directions to the payload index, neutral, and extreme value.
payload_axis = {}

"""
From the manual
The Control Stick's coordinate positions stick_x and stick_y
are signed characters with the range of -128 ~ 127.
However, for the actual program we recommend using values
within the ranges shown below:

Left/right X axis   +/- 61
Up/down Y axis      +/- 63
X axis incline      +/- 45
Y axis incline      +/- 47
"""


# The tuple will mean: payload_idx, left extreme, left mid, left zero, right zero, right mid, right extreme.
# Idealized
payload_axis['STICK_LEFT_RIGHT']  = AxisPoints(3, 128 - 63, 128 - 47, 128 - 31, 128 + 31, 128 + 47, 128 + 63)
payload_axis['STICK_UP_DOWN']     = AxisPoints(4, 128 - 61, 128 - 45, 128 - 31, 128 + 31, 128 + 45, 128 + 61)
# Bumping these values to try and make things "feel right"?
# payload_axis['STICK_LEFT_RIGHT']  = AxisPoints(3, 128 - 103, 128 - 57, 128 - 31, 128 + 31, 128 + 57, 128 + 103)
# payload_axis['STICK_UP_DOWN']     = AxisPoints(4, 128 - 101, 128 - 57, 128 - 31, 128 + 31, 128 + 57, 128 + 101)

# Mapping from the button direction to its associated index in port->axis, neutral value, and extreme value.
adapter_axis = {}

# Mapping from the button direction to its associated index in port->axis, left-extreme, left-ramp, left-min, right-min, right-ramp, right-extreme
# Mapping from the button direction to its associated index in port->axis, up-extreme, up-ramp, up-min, down-min, down-ramp, down-extreme
# Saturation values should be -80 to 80
adapter_axis['STICK_LEFT_RIGHT']  = AxisPoints(0, 128 - 80, 128 - 40, 128 + 0, 128 + 0, 128 + 40, 128 + 80)
adapter_axis['STICK_UP_DOWN']     = AxisPoints(1, 128 + 80, 128 + 40, 128 + 0, 128 - 0, 128 - 40, 128 - 80)


SYMBOLIC = 1
if SYMBOLIC:
    LR_src1, LR_src2, LR_src3, LR_src4, LR_src5, LR_src6 = sym.symbols('LR_src1, LR_src2, LR_src3, LR_src4, LR_src5, LR_src6')
    UD_src1, UD_src2, UD_src3, UD_src4, UD_src5, UD_src6 = sym.symbols('UD_src1, UD_src2, UD_src3, UD_src4, UD_src5, UD_src6')
    payload_axis['STICK_LEFT_RIGHT']  = AxisPoints(3, LR_src1, LR_src2, LR_src3, LR_src4, LR_src5, LR_src6)
    payload_axis['STICK_UP_DOWN']     = AxisPoints(4, UD_src1, UD_src2, UD_src3, UD_src4, UD_src5, UD_src6)

    LR_dst1, LR_dst2, LR_dst3, LR_dst4, LR_dst5, LR_dst6 = sym.symbols('LR_dst1, LR_dst2, LR_dst3, LR_dst4, LR_dst5, LR_dst6')
    UD_dst1, UD_dst2, UD_dst3, UD_dst4, UD_dst5, UD_dst6 = sym.symbols('UD_dst1, UD_dst2, UD_dst3, UD_dst4, UD_dst5, UD_dst6')
    adapter_axis['STICK_LEFT_RIGHT']  = AxisPoints(0, LR_dst1, LR_dst2, LR_dst3, LR_dst4, LR_dst5, LR_dst6)
    adapter_axis['STICK_UP_DOWN']     = AxisPoints(1, UD_dst1, UD_dst2, UD_dst3, UD_dst4, UD_dst5, UD_dst6)


# payload_defaults = {
#     #
#     LR_src1: 128 - 63,
#     LR_src2: 128 - 47,
#     LR_src3: 128 - 31,
#     LR_src4: 128 + 31,
#     LR_src5: 128 + 47,
#     LR_src6: 128 + 63,
#     #
#     UD_src1: 128 + 61,
#     UD_src2: 128 + 45,
#     UD_src3: 128 + 31,
#     UD_src4: 128 - 31,
#     UD_src5: 128 - 45,
#     UD_src6: 128 - 61,
# }


EVALF = 0

with sym.evaluate(EVALF):

    # I find that 6272 works well for my xbox controller
    # 4960 is the default.
    configDeadZone = sym.symbols('configDeadZone')

    src_width = int(2 ** 7)
    dst_width = int(2 ** 15)
    configDeadFrac = configDeadZone / dst_width
    configDeadAdj = src_width * configDeadFrac

    mid = sym.Integer(128)
    payload_defaults = {
        #
        LR_src1: mid - 103,
        LR_src2: mid - 57,
        LR_src3: mid - configDeadAdj,
        LR_src4: mid + configDeadAdj,
        LR_src5: mid + 57,
        LR_src6: mid + 103,
        #
        UD_src1: mid - 101,
        UD_src2: mid - 57,
        UD_src3: mid - configDeadAdj,
        UD_src4: mid + configDeadAdj,
        UD_src5: mid + 57,
        UD_src6: mid + 101,
    }

    adapter_defaults = {
        #
        LR_dst1: mid - 80,
        LR_dst2: mid - 40,
        LR_dst3: mid - 0,
        LR_dst4: mid + 0,
        LR_dst5: mid + 40,
        LR_dst6: mid + 80,
        #
        UD_dst1: mid + 80,
        UD_dst2: mid + 40,
        UD_dst3: mid + 0,
        UD_dst4: mid - 0,
        UD_dst5: mid - 40,
        UD_dst6: mid - 80,
    }

symbolic_defaults = {
    **adapter_defaults,
    **payload_defaults,
    configDeadZone: 4960,
}

# The following will generate code to builds the port->buttons data from payload.
lines = []
button_parts = []
for k, (idx, val) in payload_buttons.items():
    adapt_val = adapter_buttons[k]
    if val > adapt_val:
        shift = int(math.log2(val / adapt_val))
        button_parts.append(f' (uint16_t) (payload[{idx}] & {val:#04x}) >> {shift} |  // map {k} to {adapt_val:#04x} ')
    else:
        shift = int(math.log2(adapt_val / val))
        button_parts.append(f' (uint16_t) (payload[{idx}] & {val:#04x}) << {shift} |  // map {k} to {adapt_val:#04x}')
lines.append('uint16_t btns = (')
lines.append('\n'.join(button_parts))
lines.append(');')

### In progress writing better logic
### This logic gets us most of the way there, but we do a bit more manual munging after


lines.append('\n// Source payload magnitudes to be mapped')
for k, d in payload_defaults.items():
    if EVALF:
        d = d.subs(symbolic_defaults)
    lines.append(f'float {k} = {d};'.replace('configDeadZone', '((float) configDeadZone)'))

lines.append('\n// Destination adapater magnitudes to map to')
for k, d in adapter_defaults.items():
    if EVALF:
        d = d.subs(symbolic_defaults)
    lines.append(f'float {k} = {d};'.replace('configDeadZone', '((float) configDeadZone)'))

lines.append('\n// Default to a zero position')
lines.append(f'uint8_t stick_lr_val = (uint8_t) {adapter_axis["STICK_LEFT_RIGHT"][3]};')
lines.append(f'uint8_t stick_ud_val = (uint8_t) {adapter_axis["STICK_UP_DOWN"][3]};')
for key, payload_tup in payload_axis.items():
    adapt_tup = adapter_axis[key]
    if adapt_tup is not NotImplemented:
        idx1, *vals1 = payload_tup
        idx2, *vals2 = adapt_tup
        p = sym.symbols('VAL')

        if key in ['STICK_LEFT_RIGHT']:
            var = 'stick_lr_val'
        else:
            var = 'stick_ud_val'

        v1 = vals1[0]
        v2 = vals2[0]
        if EVALF:
            v1 = v1.subs(symbolic_defaults)
            v2 = v2.subs(symbolic_defaults)
        raw = f'payload[{idx1}]'
        lines.append('')
        lines.append(f'// Handle {key}')
        lines.append(f'if ( {raw} < {v1} )')
        lines.append('{')
        lines.append(f'    {var} = {v2};  // Handle {key} extreme low')
        lines.append('}')
        for i in range(len(vals1) - 1):
            v11 = vals1[i]
            v12 = vals1[i + 1]
            v21 = vals2[i]
            v22 = vals2[i + 1]

            range1 = v12 - v11
            range2 = v22 - v21
            el = '' if i == 0 else 'else '
            r = (((p - v11) / range1) * range2 + v21).evalf()
            if EVALF:
                r = r.subs(symbolic_defaults)
            expr = repr(r).replace('VAL', f'((float) payload[{idx1}])')

            if EVALF:
                v12 = v12.subs(symbolic_defaults)

            lines.append(f'else if ( {raw} < {v12} )')
            lines.append('{')
            lines.append(f'    {var} = (uint8_t) ({expr});  // Handle {key} ramp')
            lines.append('}')
        v1 = vals1[-1]
        v2 = vals2[-1]
        lines.append('else')
        lines.append('{')
        lines.append(f'    {var} = {v2};  // Handle {key} extreme high')
        lines.append('}')


print(textwrap.indent(chr(10).join(lines), '    '))
