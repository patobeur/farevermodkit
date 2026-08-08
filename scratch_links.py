import os

def add_links(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    # The target string
    target = '<li>Pour consulter les donn&eacute;es hors du jeu, ouvrir <a href="farever-report.html">farever-report.html</a>.</li>'
    
    # The new links to append
    new_links = '<li>Acc&eacute;der au <a href="../">Dossier des donn&eacute;es</a> du rapport.</li><li>Acc&eacute;der au <a href="../../../../">Dossier principal du Mod (FMK)</a>.</li>'
    
    if target in text:
        text = text.replace(target, target + new_links)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print(f'Updated {path}')
    else:
        print(f'Target not found in {path}')

add_links(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\index.html')
