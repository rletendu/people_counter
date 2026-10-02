# Compteur de passages (Arduino Nano + ultrasons + TM1637)

Boîtier autonome : un capteur à ultrasons fixé sur un côté du sas compte les passages, le total s'affiche sur un afficheur 7 segments 4 digits (TM1637). Pas de réseau, pas de RTC, pas de SD.

## Matériel

| Composant | Remarque |
|---|---|
| Arduino Nano | ATmega328P |
| HC-SR04 (intérieur) ou JSN-SR04T (robuste, zone morte ~20 cm) | |
| Afficheur 7 segments 4 digits TM1637 | module 4 fils |
| Bouton poussoir | remise à zéro |
| Alimentation | chargeur USB ou powerbank (~50 mA) |

## Câblage

| Capteur / module | Broche Nano |
|---|---|
| HC-SR04 VCC | 5V |
| HC-SR04 GND | GND |
| HC-SR04 TRIG | D9 |
| HC-SR04 ECHO | D10 |
| TM1637 VCC | 5V |
| TM1637 GND | GND |
| TM1637 CLK | D4 |
| TM1637 DIO | D5 |
| Bouton (une patte) | D3 |
| Bouton (autre patte) | GND |

Le bouton utilise `INPUT_PULLUP`, aucune résistance externe n'est nécessaire.

## Compilation et téléversement

```bash
pio run                      # compiler
pio run -t upload            # téléverser
pio device monitor           # moniteur série (9600 bauds)
```

Au premier build, PlatformIO installe automatiquement la bibliothèque d'affichage `smougenot/TM1637` définie dans `platformio.ini`.

Deux environnements sont fournis selon le bootloader de la carte :

```bash
pio run -e nano_new -t upload   # Nano d'origine (bootloader récent)
pio run -e nano_old -t upload   # la plupart des clones CH340
```

Si le téléversement échoue avec l'erreur `not in sync`, essaie l'autre environnement.

## Fonctionnement

- **Calibration** au démarrage : le boîtier mesure la distance au mur d'en face et fixe le seuil de détection à cette distance moins `MARGIN_CM` (20 cm). **Allume-le sas vide.** Pendant la calibration, le deux-points de l'afficheur est allumé.
- **Détection** : deux mesures concordantes sont nécessaires pour changer d'état (anti-parasites). Un passage est compté à l'arrivée de la personne.
- **Blocage** : si quelque chose reste plus de 10 s devant le capteur, le comptage est suspendu jusqu'à ce que le passage se libère. Le **deux-points allumé** indique que le capteur est bloqué.
- **Remise à zéro** : bouton sur D3.

## Réglages

Tout est regroupé en haut de `src/main.cpp` :

| Constante | Rôle | Défaut |
|---|---|---|
| `MARGIN_CM` | marge sous la distance au mur pour détecter | 20 |
| `CONFIRM_READS` | lectures concordantes requises | 2 |
| `MAX_PRESENCE_MS` | délai avant l'état « bloqué » | 10000 |
| `LOOP_DELAY_MS` | pause entre deux mesures | 40 |

## Entrées vs passages

L'affichage montre le nombre brut de passages. Si chaque visiteur entre puis ressort, les entrées valent environ `passages / 2` : remplace `passCount` par `passCount / 2` dans les deux appels d'affichage. Le brut a l'avantage de révéler qu'une personne est encore à l'intérieur quand le chiffre est impair.

## Limites connues

- Le capteur est placé à environ 1 m du sol et vise un mur à moins de 1,5 m : au-delà, l'absence d'écho se confond avec un passage.
- Deux personnes côte à côte sont comptées une seule fois.
- Les vêtements épais absorbent l'écho et peuvent provoquer des passages manqués.
- Le total est perdu en cas de coupure de courant (pas d'EEPROM, pas de SD).
