import re
s = '"history": [{"ms": 350000}, {"ms": 340000}, {"ms": 325204}],'
m = re.search(r'"history"\s*:\s*(\[[^\]]*\])', s)
print(m.group(1) if m else 'NO MATCH')
