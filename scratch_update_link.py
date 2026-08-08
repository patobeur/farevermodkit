import os

def update_link(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    old_link = '<li>Acc&eacute;der au <a href="../../../../">Dossier principal du Mod (FMK)</a>.</li>'
    new_link = '<li>Acc&eacute;der au <a href="../../../../">Dossier d\'installation du mod (FMK) dans Steam</a>.</li>'
    
    if old_link in text:
        text = text.replace(old_link, new_link)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print("Updated!")
    else:
        print("Not found!")

update_link(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\index.html')
