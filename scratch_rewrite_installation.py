import os
import re

def rewrite_installation(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    new_section = '''<section id="installation"><h2>Installation (Pour les joueurs)</h2><p>Vous souhaitez simplement utiliser le mod ? C'est très simple !</p><ol><li>Téléchargez la dernière <b>Release</b> depuis GitHub.</li><li>Extrayez le contenu de l'archive.</li><li>Copiez l'intégralité du contenu extrait directement dans le dossier d'installation de votre jeu Farever (là où se trouve <code>Farever.exe</code>).</li></ol><p class="warn"><b>Attention :</b> Ne pas utiliser simultanément <em>farever-minimap</em> et ce mod. Les deux installent un overlay (via <code>dxgi.dll</code>) qui va entrer en conflit.</p></section>'''

    old_section_match = re.search(r'<section id="installation">.*?</section>', text, flags=re.DOTALL)
    if old_section_match:
        text = text.replace(old_section_match.group(0), new_section)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print("Updated mod.html installation")
    else:
        print("Installation section not found in mod.html")

rewrite_installation(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\mod.html')
