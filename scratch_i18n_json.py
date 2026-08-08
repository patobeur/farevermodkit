import json, os

path = r'd:\farever-mods\farevermodkit\modules\Patobeur\Report\languages'

en = {
    "status": "Local HTML report by account and character.",
    "btn_wait_char": "Waiting for a character...",
    "btn_wait_data": "Waiting for character data...",
    "btn_export_req": "Export requested for",
    "btn_export_unav": "Export unavailable",
    "btn_save": "Save now",
    "btn_open": "Open report",
    "btn_last_none": "Last save: none this session",
    "btn_last_now": "Last save: just now",
    "btn_last_ago": "Last save: ",
    "btn_sec": "s ago",
    "settings_desc": "The report is automatically updated for each detected character."
}

fr = {
    "status": "Rapport HTML local par compte et personnage.",
    "btn_wait_char": "En attente d'un personnage...",
    "btn_wait_data": "En attente des donnees du personnage...",
    "btn_export_req": "Export demande pour",
    "btn_export_unav": "Export indisponible",
    "btn_save": "Sauvegarder maintenant",
    "btn_open": "Ouvrir le rapport",
    "btn_last_none": "Derniere sauvegarde : aucune cette session",
    "btn_last_now": "Derniere sauvegarde : a l'instant",
    "btn_last_ago": "Derniere sauvegarde : il y a ",
    "btn_sec": " s",
    "settings_desc": "Le rapport est actualise automatiquement pour chaque personnage detecte."
}

es = {
    "status": "Reporte HTML local por cuenta y personaje.",
    "btn_wait_char": "Esperando un personaje...",
    "btn_wait_data": "Esperando datos del personaje...",
    "btn_export_req": "Exportacion solicitada para",
    "btn_export_unav": "Exportacion no disponible",
    "btn_save": "Guardar ahora",
    "btn_open": "Abrir reporte",
    "btn_last_none": "Ultima guardia: ninguna esta sesion",
    "btn_last_now": "Ultima guardia: hace un instante",
    "btn_last_ago": "Ultima guardia: hace ",
    "btn_sec": " s",
    "settings_desc": "El reporte se actualiza automaticamente para cada personaje detectado."
}

with open(os.path.join(path, 'en-US.json'), 'w', encoding='utf-8') as f:
    json.dump(en, f, indent=2)

with open(os.path.join(path, 'fr-FR.json'), 'w', encoding='utf-8') as f:
    json.dump(fr, f, indent=2)

with open(os.path.join(path, 'es-ES.json'), 'w', encoding='utf-8') as f:
    json.dump(es, f, indent=2)
