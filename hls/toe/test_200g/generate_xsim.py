#!/usr/bin/env python3
"""Declare the actual HLS RTL ports; the independent SV TB supplies behavior."""
import pathlib
import re
import sys
import shutil
import os
import hashlib
here = pathlib.Path(__file__).resolve().parent
width = int(sys.argv[1])
rtl = here / f'work_{width}/solution/syn/verilog'
case = os.environ.get('TOE_CASE', '')
component = case == 'tx_sequence_wrap'
top_module = 'toe_tx_sar_table' if component else 'toe'
top = (rtl / f'{top_module}.v').read_text()
ports = re.findall(r'^(input|output)\s+(?:wire\s+|reg\s+)?(\[[^\]]+\]\s*)?(\w+)\s*;', top, re.M)
assert ports, 'No RTL ports found'
if case and not re.fullmatch('[a-z_]+', case):
    raise ValueError('invalid TOE_CASE')
out = here / (f'xsim_{width}_{case}' if case else f'xsim_{width}')
out.mkdir(exist_ok=True)
for data in rtl.glob('*.dat'):
    shutil.copy2(data, out/data.name)
text = '`timescale 1ns/1ps\nmodule tb_toe;\n'
text += f'localparam W={width*8}; localparam K={width};\n'
for direction, size, name in ports:
    text += f'logic {size or ""}{name};\n'
text += top_module+' dut (\n' + ',\n'.join(f'.{n}({n})' for _, _, n in ports) + '\n);\n'
# Each AXIS input is queued; outputs are sampled once per rising edge.
channels = {}
for d,s,n in ports:
    if n.endswith('_TDATA'):
        prefix=n[:-6]; fields={n2.split(prefix+'_',1)[1]:(d2,s2) for d2,s2,n2 in ports if n2.startswith(prefix+'_')}
        channels[prefix]=(d,fields)
for prefix,(d,fields) in channels.items():
    if d=='input':
        text += f'typedef struct packed {{\n'
        for field,(direction,size) in fields.items():
            if field in ('TDATA','TKEEP','TSTRB','TLAST'):
                text += f'logic {size or ""}{field};\n'
        text += f'}} {prefix}_word;\n{prefix}_word {prefix}_q[$];\n'
text += 'task automatic drive_inputs;\n'
for prefix,(d,fields) in channels.items():
    if d=='input':
        text += f'{prefix}_TVALID=({prefix}_q.size()!=0);\n'
        for field in fields:
            if field in ('TDATA','TKEEP','TSTRB','TLAST'):
                text += f'{prefix}_{field}={prefix}_q.size()?{prefix}_q[0].{field}:\'0;\n'
    else:
        text += f'{prefix}_TREADY=1;\n'
text += 'endtask\ntask automatic pop_inputs;\n'
for prefix,(d,fields) in channels.items():
    if d=='input':
        text += f'if({prefix}_TVALID && {prefix}_TREADY) void\'({prefix}_q.pop_front());\n'
text += 'endtask\n'
if case and not component:
    text += f'localparam string CASE_NAME=\"{case}\";\n'
    text += 'int seed=1,profile=0,target=-1,stall_cycles=0,position=0;int hold_left=0;bit hold_used=0;\n'
    inputs = [p for p, (d, _) in channels.items() if d == 'input']
    outputs = [p for p, (d, _) in channels.items() if d == 'output']
    text += f'localparam NUM_OUTPUTS={len(outputs)};\n'
    text += f'bit visible[{len(inputs)}]; int stall_hits[{len(outputs)}]; int beats[{len(outputs)}]; int frame_beat[{len(outputs)}];\n'
    text += f'task automatic init_stalls; for(int i=0;i<{len(outputs)};i++)begin stall_hits[i]=0;beats[i]=0;frame_beat[i]=0;end endtask\n'
    text += 'task automatic apply_stalls;\n'
    for i, p in enumerate(inputs):
        text += f'if(!visible[{i}] && {p}_TVALID && permit({i},1))visible[{i}]=1; {p}_TVALID &= visible[{i}];\n'
    for i, p in enumerate(outputs):
        text += f'{p}_TREADY &= permit({i},0);\n'
        last = f'{p}_TLAST' if 'TLAST' in channels[p][1] else "1'b1"
        trigger = f'((position==0 && beats[{i}]==0)||(position==1 && frame_beat[{i}]==1 && !{last})||(position==2 && frame_beat[{i}]>0 && {last}))'
        text += f'if(target=={i} && stall_cycles>0)begin if(!hold_used && {p}_TVALID && {trigger})begin hold_left=stall_cycles;hold_used=1;end if(hold_left>0)begin {p}_TREADY=0;hold_left--;end end\n'
    text += 'endtask\ntask automatic observe_stalls;\n'
    for i, p in enumerate(inputs):
        text += f'if({p}_TVALID && {p}_TREADY)visible[{i}]=0;\n'
    for i, p in enumerate(outputs):
        last = f'{p}_TLAST' if 'TLAST' in channels[p][1] else "1'b1"
        text += f'if({p}_TVALID && !{p}_TREADY)stall_hits[{i}]++;if({p}_TVALID && {p}_TREADY)begin beats[{i}]++;frame_beat[{i}]={last} ? 0 : frame_beat[{i}]+1;end\n'
    text += 'endtask\ntask automatic report_coverage;\n'
    for i, p in enumerate(outputs):
        text += f'$display("COVER {p} beats=%0d stalled=%0d",beats[{i}],stall_hits[{i}]);\n'
    for i,p in enumerate(inputs):
        text += f'$display("QUEUE {p} pending=%0d valid=%b ready=%b",{p}_q.size(),{p}_TVALID,{p}_TREADY);\n'
    text += 'endtask\ntask automatic check_coverage; report_coverage();\n'
    text += 'if(target>=0 && stall_cycles>0 && !hold_used)$fatal(1,"directed stall not triggered");\n'
    text += 'if(target>=0 && (stall_cycles>0 || profile>0) && stall_hits[target]==0)$fatal(1,"target stall not exercised");\nendtask\n'
    (out/'channels.txt').write_text('\n'.join(f'{i} {p}' for i,p in enumerate(outputs))+'\n')
# AXI stability checks apply equally to data and metadata channels.
for prefix,(_,fields) in channels.items():
    payload=','.join(prefix+'_'+f for f in fields if f in ('TDATA','TKEEP','TLAST'))
    text += f'assert property (@(posedge ap_clk) disable iff (!ap_rst_n) {prefix}_TVALID && !{prefix}_TREADY |=> {prefix}_TVALID && $stable({{{payload}}})) else $fatal(1,"AXIS unstable: {prefix}");\n'
if component:
    text += 'task automatic component_defaults;\n'
    for d, size, name in ports:
        if d == 'input' and name != 'ap_clk':
            value = 1 if name.endswith('full_n') or name in ('ap_start','ap_continue') else 0
            text += f'{name}={value};\n'
    text += 'endtask\n'+(here/f'cases/tb_{case}.svh').read_text()
elif case:
    text += '`include \"common/xsim.svh\"\n'
    text += (here/'common/extended.svh').read_text()
    text += (here/f'cases/tb_{case}.svh').read_text()
else:
    text += (here/'tb_xsim.svh').read_text()
text += '\nendmodule\n'
(out/'tb_toe.sv').write_text(text)
(out/'sources.f').write_text('\n'.join(str(p) for p in sorted(rtl.glob('*.v')))+'\n'+str(out/'tb_toe.sv')+'\n')
(out/'ports.txt').write_text('\n'.join(f'{d} {s or ""}{n}' for d,s,n in ports))

if case:
    digest = hashlib.sha256(text.encode())
    for p in sorted(rtl.glob('*')):
        if p.suffix in ('.v', '.dat'):
            digest.update(p.read_bytes())
    digest.update((here/'common/xsim.svh').read_bytes())
    (out/'inputs.sha256').write_text(digest.hexdigest()+'\n')
