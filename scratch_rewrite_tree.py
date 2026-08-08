import os
import re

def rewrite_tree(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    new_section = '''<section id="fichiers"><h2>Organisation des fichiers apr&egrave;s installation</h2><pre class="tree">Dossier du jeu (Installation) :
Farever\\
|-- Farever.exe
|-- dxgi.dll                 obligatoire &agrave; la racine
`-- farevermodkit\\
    |-- modules\\
    |   `-- Patobeur\\
    |       `-- Report\\
    |           `-- html\\
    |               |-- index.html
    |               |-- mod.html
    |               `-- farever-report.html
    `-- assets\\
        `-- atlas\\
            |-- farever-atlas.tsv
            `-- farever-atlas-icons.dds

Dossier des donn&eacute;es (%LOCALAPPDATA%\\farevermodkit) :
farevermodkit\\
|-- farever-report-data.js   g&eacute;n&eacute;r&eacute; par le jeu
|-- farever-nav-state.txt
|-- farever-routes.txt
|-- html\\                     fichiers UI copi&eacute;s
|   |-- index.html
|   |-- mod.html
|   `-- farever-report.html
|-- data\\
|   `-- accounts\\
|       |-- farever-collection.json
|       |-- farever-inventory-&lt;personnage&gt;.json
|       `-- farever-jobs-&lt;personnage&gt;.json
|-- settings\\
|   `-- farever-modkit.ini
`-- logs\\
    `-- farever-modkit.log</pre><p>Les donn&eacute;es sont locales. La page HTML n'utilise aucun serveur et aucune connexion Internet.</p></section>'''

    # Find the old section
    old_section_match = re.search(r'<section id="fichiers">.*?</section>', text, flags=re.DOTALL)
    if old_section_match:
        text = text.replace(old_section_match.group(0), new_section)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print("Updated mod.html tree")
    else:
        print("Section not found in mod.html")

rewrite_tree(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\mod.html')
