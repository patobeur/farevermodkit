import re

path = r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\farever-report.html'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

# Fix syntax error <summary>t('Level') -> <summary>' + t('Level')
text = text.replace("<summary>t('Level')", "<summary>' + t('Level')")

# Fix Dernière lecture
text = re.sub(r"'Derni[^\']+lecture : '", "t('LastRead') + ' : '", text)
text = text.replace("Dernière lecture : '", "t('LastRead') + ' : '")
text = text.replace("Dernire lecture : '", "t('LastRead') + ' : '")

with open(path, 'w', encoding='utf-8') as f:
    f.write(text)

print('Fixed syntax errors')
