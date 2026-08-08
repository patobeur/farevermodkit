import os

def remove_buttons(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    target = '''<div style="display:flex; gap:10px; align-items:center; margin-left:auto;">
    <a href="../../../../" target="_blank" style="display:inline-block; padding: 6px 14px; background: #24292f; color: #ffffff !important; border-radius: 20px; text-decoration: none; font-size: 0.9em; box-shadow: 0 2px 5px rgba(0,0,0,0.2); transition: transform 0.2s ease;" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">📁 Dossier Mod (FMK)</a>
    <a href="../" target="_blank" style="display:inline-block; padding: 6px 14px; background: #24292f; color: #ffffff !important; border-radius: 20px; text-decoration: none; font-size: 0.9em; box-shadow: 0 2px 5px rgba(0,0,0,0.2); transition: transform 0.2s ease;" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">📁 Dossier Donn&eacute;es (Report)</a>
</div>'''

    if target in text:
        text = text.replace(target, '')
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print("Removed from HTML")
    else:
        print("Target not found in HTML")

remove_buttons(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\farever-report.html')
