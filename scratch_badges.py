import os
import re

def add_badge(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    # Define the badge style
    badge_style = "display:inline-block; padding: 4px 12px; background: #24292f; color: #ffffff !important; border-radius: 20px; text-decoration: none; font-size: 0.85em; box-shadow: 0 2px 5px rgba(0,0,0,0.2); transition: transform 0.2s ease;"
    
    # Replace the old a tags with new badged ones (adding a little link icon emoji for extra clarity)
    text = text.replace(
        '<h3><a href="https://github.com/Blaakan/farever-mods" target="_blank" style="color:inherit;text-decoration:none;">Blaakan</a></h3>',
        f'<h3><a href="https://github.com/Blaakan/farever-mods" target="_blank" style="{badge_style}" onmouseover="this.style.transform=\'scale(1.05)\'" onmouseout="this.style.transform=\'scale(1)\'">Blaakan 🔗</a></h3>'
    )
    
    text = text.replace(
        '<h3><a href="https://github.com/ramisotti13-eng/farever-minimap" target="_blank" style="color:inherit;text-decoration:none;">ramisotti13-eng</a></h3>',
        f'<h3><a href="https://github.com/ramisotti13-eng/farever-minimap" target="_blank" style="{badge_style}" onmouseover="this.style.transform=\'scale(1.05)\'" onmouseout="this.style.transform=\'scale(1)\'">ramisotti13-eng 🔗</a></h3>'
    )
    
    text = text.replace(
        '<h3><a href="https://github.com/Brudr" target="_blank" style="color:inherit;text-decoration:none;">Brudr</a></h3>',
        f'<h3><a href="https://github.com/Brudr" target="_blank" style="{badge_style}" onmouseover="this.style.transform=\'scale(1.05)\'" onmouseout="this.style.transform=\'scale(1)\'">Brudr 🔗</a></h3>'
    )

    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)

add_badge(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\index.html')
add_badge(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\mod.html')
print('Badges added!')
