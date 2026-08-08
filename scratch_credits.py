import os
import re

def fix_credits(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    # 1. Remove patobeur
    # The card looks like: <article class="card"><h3>Patobeur</h3>...!</p></article>
    text = re.sub(r'<article class="card"><h3>Patobeur</h3>.*?</article>', '', text, flags=re.DOTALL)
    
    # 2. Add links to others
    text = text.replace('<h3>Blaakan</h3>', '<h3><a href="https://github.com/Blaakan/farever-mods" target="_blank" style="color:inherit;text-decoration:none;">Blaakan</a></h3>')
    text = text.replace('<h3>ramisotti13-eng</h3>', '<h3><a href="https://github.com/ramisotti13-eng/farever-minimap" target="_blank" style="color:inherit;text-decoration:none;">ramisotti13-eng</a></h3>')
    text = text.replace('<h3>Brudr</h3>', '<h3><a href="https://github.com/Brudr" target="_blank" style="color:inherit;text-decoration:none;">Brudr</a></h3>')
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)

fix_credits(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\index.html')
fix_credits(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\mod.html')
print('Credits updated!')
