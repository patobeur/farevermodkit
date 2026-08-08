import re

def add_i18n(path):
    with open(path, 'r', encoding='utf-8') as f:
        html = f.read()

    # The HTML part
    html = html.replace('placeholder="Rechercher..."', 'placeholder="Rechercher / Search..."')
    html = html.replace('Chargement automatique de', 'Chargement automatique / Auto-loading of')
    html = html.replace("En attente d'un personnage", "En attente / Waiting for character")

    # JS part
    js_match = re.search(r'<script>(.*?)</script>', html, flags=re.DOTALL)
    if not js_match:
        return
    js = js_match.group(1)

    i18n_code = """
let LANG = navigator.language.startsWith('fr') ? 'fr' : 'en';
const i18n = {
    fr: {
        'Inv': 'Inventaire', 'Bank': 'Banque commune', 'Equip': '\u00c9quipement', 'Jobs': 'M\u00e9tiers', 'Runes': 'Runes', 'Mastery': 'Ma\u00eetrise', 'Collection': 'Collection',
        'Rarity0': 'Commune', 'Rarity1': 'Peu commune', 'Rarity2': 'Rare', 'Rarity3': 'Epique', 'Rarity4': 'Legendaire',
        'Type': 'Type', 'Slot': 'Emplacement', 'Class': 'Classe', 'Product': 'Objet produit', 'Ingredients': 'Ingr&eacute;dients',
        'Cost': 'Co&ucirc;t', 'Find': 'Recette &agrave; trouver', 'Auto': 'Automatique', 'Sold': 'Vendue par un marchand',
        'Empty': 'Aucune donn\u00e9e \u00e0 afficher.', 'Level': 'Niveau', 'Upgrade': 'Am\u00e9lioration', 'Rarity': 'Raret\u00e9',
        'Image': 'Image n&deg;', 'NoJob': 'Aucun m\u00e9tier avec des recettes.', 'Recipe': 'recette', 'Recipes': 'recettes',
        'UnknownClass': 'Classe inconnue', 'LastRead': 'Derni\u00e8re lecture', 'ActiveWpn': 'Arme active', 'Kills': 'victimes'
    },
    en: {
        'Inv': 'Inventory', 'Bank': 'Shared Bank', 'Equip': 'Equipment', 'Jobs': 'Professions', 'Runes': 'Runes', 'Mastery': 'Mastery', 'Collection': 'Collection',
        'Rarity0': 'Common', 'Rarity1': 'Uncommon', 'Rarity2': 'Rare', 'Rarity3': 'Epic', 'Rarity4': 'Legendary',
        'Type': 'Type', 'Slot': 'Slot', 'Class': 'Class', 'Product': 'Produced Item', 'Ingredients': 'Ingredients',
        'Cost': 'Cost', 'Find': 'Recipe to find', 'Auto': 'Automatic', 'Sold': 'Sold by merchant',
        'Empty': 'No data to display.', 'Level': 'Level', 'Upgrade': 'Upgrade', 'Rarity': 'Rarity',
        'Image': 'Image #', 'NoJob': 'No profession with recipes.', 'Recipe': 'recipe', 'Recipes': 'recipes',
        'UnknownClass': 'Unknown Class', 'LastRead': 'Last read', 'ActiveWpn': 'Active weapon', 'Kills': 'kills'
    }
};
const t = (k) => i18n[LANG][k] || k;
"""

    # Replace hardcoded strings
    js = js.replace("['Inventaire','Banque commune','\\u00c9quipement','M\\u00e9tiers','Runes','Ma\\u00eetrise','Collection']", "[t('Inv'),t('Bank'),t('Equip'),t('Jobs'),t('Runes'),t('Mastery'),t('Collection')]")
    js = js.replace("counts={'Inventaire'", "counts={[t('Inv')]")
    js = js.replace("'Banque commune':", "[t('Bank')]:")
    js = js.replace("'\\u00c9quipement':", "[t('Equip')]:")
    js = js.replace("'M\\u00e9tiers':", "[t('Jobs')]:")
    js = js.replace("'Runes':", "[t('Runes')]:")
    js = js.replace("'Ma\\u00eetrise':", "[t('Mastery')]:")

    js = js.replace("if(tab==='Inventaire')", "if(tab===t('Inv'))")
    js = js.replace("if(tab==='\\u00c9quipement')", "if(tab===t('Equip'))")
    js = js.replace("if(tab==='Banque commune')", "if(tab===t('Bank'))")
    js = js.replace("if(tab==='M\\u00e9tiers')", "if(tab===t('Jobs'))")
    js = js.replace("if(tab==='Runes')", "if(tab===t('Runes'))")
    js = js.replace("if(tab==='Ma\\u00eetrise')", "if(tab===t('Mastery'))")
    js = js.replace("if(tab==='Collection')", "if(tab===t('Collection'))")

    js = js.replace("['Commune','Peu commune','Rare','Epique','Legendaire']", "[t('Rarity0'),t('Rarity1'),t('Rarity2'),t('Rarity3'),t('Rarity4')]")
    
    js = js.replace("'Type : '+esc(x)", "t('Type') + ' : ' + esc(x)")
    js = js.replace("'Emplacement : '+esc(x)", "t('Slot') + ' : ' + esc(x)")
    js = js.replace("'Classe : '+esc(x)", "t('Class') + ' : ' + esc(x)")
    
    js = js.replace("<h4>Objet produit</h4>", "<h4>'+t('Product')+'</h4>")
    js = js.replace("<b>Ingr&eacute;dients</b>", "<b>'+t('Ingredients')+'</b>")
    js = js.replace("'Co&ucirc;t : '+esc", "t('Cost') + ' : ' + esc")
    
    js = js.replace("Recette &agrave; trouver", "'+t('Find')+'")
    js = js.replace("Automatique", "'+t('Auto')+'")
    js = js.replace(">Vendue par un marchand<", ">'+t('Sold')+'<")
    
    js = js.replace("Aucune donn\\u00e9e \\u00e0 afficher.", "'+t('Empty')+'")
    js = js.replace("Aucun m\\u00e9tier avec des recettes.", "'+t('NoJob')+'")
    
    js = js.replace("'Niveau '+x.level", "t('Level') + ' ' + x.level")
    js = js.replace("'Am\\u00e9lioration +'+x.upgrade", "t('Upgrade') + ' +' + x.upgrade")
    js = js.replace("'Raret\\u00e9 '+x.rarity", "t('Rarity') + ' ' + x.rarity")
    js = js.replace("'Image n&deg; '+esc(a.icon)", "t('Image') + ' ' + esc(a.icon)")
    
    js = js.replace("Classe inconnue", "'+t('UnknownClass')+'")
    js = js.replace("'DerniÃ¨re lecture : '", "t('LastRead') + ' : '")
    js = js.replace("Arme active", "'+t('ActiveWpn')+'")
    js = js.replace(" victimes<", " ' + t('Kills') + '<")
    
    js = js.replace("Niveau '+(lv||'?')+' &mdash; '", "t('Level') + ' ' + (lv||'?') + ' &mdash; '")
    js = js.replace("+' recette'+(it.length>1?'s':'')", "+ ' ' + (it.length>1?t('Recipes'):t('Recipe'))")
    js = js.replace("niv. '", "lvl '")

    # Inject i18n_code at the start of the script, right after `let D=null,tab=`...
    # Actually wait, `let D=null,tab='Inventaire'` needs to be `tab=t('Inv')`, but t is not defined yet.
    # So we'll inject i18n_code at the very top of JS.
    js = i18n_code + js
    js = js.replace("tab='Inventaire'", "tab=t('Inv')")

    new_html = html[:js_match.start(1)] + js + html[js_match.end(1):]

    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_html)
    print("Injected i18n!")

add_i18n(r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\html\farever-report.html')
