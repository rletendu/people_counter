# Compteur de passages (Arduino Nano + ultrasons/ToF + TM1637)

Boîtier autonome : un capteur à ultrasons **ou** un capteur temps-de-vol VL53L1X, fixé sur un côté du sas, compte les passages ; le total s'affiche sur un afficheur 7 segments 4 digits (TM1637). Les deux capteurs sont toujours câblés et actifs, le choix se fait au boîtier via un menu à boutons (voir plus bas) et persiste en EEPROM. Pas de réseau, pas de RTC, pas de SD.

## Matériel

| Composant | Remarque |
|---|---|
| Arduino Nano | ATmega328P |
| HC-SR04 (intérieur) ou JSN-SR04T (robuste, zone morte ~20 cm) | capteur ultrason |
| VL53L1X (ex. carte Pololu) | capteur temps-de-vol, bus I2C |
| Afficheur 7 segments 4 digits TM1637 | module 4 fils |
| 2 boutons poussoir | reset / menu / aperçu distance |
| Buzzer piézo passif | bips de confirmation |
| Module diode laser 5V, basse puissance (classe 1 ou 2, <1 mW) | aide au pointage, ex. type KY-008 |
| Transistor NPN (2N2222/S8050 ou équivalent) + résistance ~1 kΩ | commande du laser depuis une sortie Nano |
| Alimentation | chargeur USB ou powerbank (~50 mA) |

## Câblage

| Capteur / module | Broche Nano |
|---|---|
| HC-SR04 VCC | 5V |
| HC-SR04 GND | GND |
| HC-SR04 TRIG | D9 |
| HC-SR04 ECHO | D10 |
| VL53L1X VCC | 5V |
| VL53L1X GND | GND |
| VL53L1X SDA | A4 |
| VL53L1X SCL | A5 |
| TM1637 VCC | 5V |
| TM1637 GND | GND |
| TM1637 CLK | D4 |
| TM1637 DIO | D5 |
| Bouton A (une patte) | D3 |
| Bouton A (autre patte) | GND |
| Bouton B (une patte) | D2 |
| Bouton B (autre patte) | GND |
| Buzzer piézo (+) | D6 |
| Buzzer piézo (-) | GND |
| Module laser VCC | 5V |
| Module laser GND | collecteur du transistor |
| Base du transistor (via résistance ~1 kΩ) | D7 |
| Émetteur du transistor | GND |

Les boutons utilisent `INPUT_PULLUP`, aucune résistance externe n'est nécessaire.
Le branchement direct sur D6 convient à un piézo passif; un buzzer électromagnétique peut nécessiter un transistor.
Le module laser consomme typiquement 20 à 40 mA, trop pour une sortie Nano pilotée en direct : D7 commande un transistor NPN qui coupe l'alimentation du module côté masse, le module restant alimenté en 5V/GND.

⚠️ **Sécurité oculaire** : n'utiliser qu'un module laser basse puissance (classe 1 ou 2, <1 mW). Le firmware ne l'allume que pendant la calibration et l'aperçu distance, jamais en continu — ne pas le câbler "toujours allumé" dans l'axe d'un passage fréquenté.

⚠️ **Niveau logique I2C** : le bus I2C du Nano fonctionne en 5V. Certains modules VL53L1X bon marché n'ont ni régulateur ni translation de niveau et attendent du 3.3V strict sur SDA/SCL — vérifie la documentation du module avant de le câbler directement sur A4/A5 ; sinon alimente-le en 3.3V ou intercale un level-shifter I2C.

## Compilation et téléversement

```bash
pio run                      # compiler
pio run -t upload            # téléverser
pio device monitor           # moniteur série (9600 bauds)
```

Au premier build, PlatformIO installe automatiquement les bibliothèques définies dans `platformio.ini` : l'affichage `smougenot/TM1637` et le capteur ToF `pololu/VL53L1X`.

Les traces série sont désactivées par défaut : le moniteur reste muet tant que `DEBUG_SERIAL` vaut `0` en haut de `src/main.cpp`. Passe-le à `1` pour obtenir distance, état de présence, blocage et compteur; remets-le à `0` ensuite, ces messages occupent une part notable de la flash et de la RAM de l'ATmega328P.

Deux environnements sont fournis selon le bootloader de la carte :

```bash
pio run -e nano_new -t upload   # Nano d'origine (bootloader récent)
pio run -e nano_old -t upload   # la plupart des clones CH340
```

Si le téléversement échoue avec l'erreur `not in sync`, essaie l'autre environnement.

## Fonctionnement

- **Calibration** au démarrage (et après tout changement de capteur via le menu) : le boîtier mesure la distance au mur d'en face avec le capteur actif et fixe le seuil de détection à cette distance moins `MARGIN_CM` (20 cm). **Allume-le sas vide.** Pendant la calibration, un serpent lumineux tourne sur le contour de l'afficheur (au moins `CALIBRATION_MIN_MS`) et le laser de pointage s'allume pour vérifier/ajuster l'alignement ; un bip confirme sa réussite et éteint le laser. Si le capteur ne renvoie aucune lecture valide (pas d'écho côté ultrason, pas de mesure valide ou capteur non détecté côté ToF), la calibration abandonne au bout de `CALIBRATION_TIMEOUT_MS` (10 s) : deux notes descendantes retentissent et `----` clignote, signe d'un problème de câblage ou de visée. Le seuil précédent est conservé (au démarrage il n'y en a pas : rien n'est compté tant qu'une calibration n'a pas réussi, via le menu ou un redémarrage).
- **Détection** : `CONFIRM_READS` mesures concordantes sont nécessaires pour valider une arrivée (anti-parasites), et le passage n'est considéré terminé qu'après `CLEAR_HOLD_MS` (0,5 s) de vide continu : un mouvement de bras, un sac ou une lecture instable pendant le même passage ne sont donc comptés qu'une fois. Contrepartie : deux personnes qui se suivent à moins d'environ 0,5 s sont comptées une seule fois. Un bip court confirme chaque passage compté à l'arrivée. Une absence de lecture valide est volontairement traitée comme une présence, pas comme un sas vide.
- **Blocage** : si quelque chose reste plus de 10 s devant le capteur, le comptage est suspendu jusqu'à ce que le passage se libère. Le **deux-points allumé** et deux notes descendantes signalent le blocage.
- **Menu (capteur + largeur du cône ToF)** : un appui long (`LONG_PRESS_MS`, 1,2 s) sur l'un ou l'autre bouton, seul, ouvre un menu — deux notes montantes le confirment. L'afficheur montre un nombre à 2 chiffres clignotant : le chiffre des dizaines choisit le capteur (`1` = ultrason, `2` = ToF) et bascule à chaque relâchement du **bouton A** ; le chiffre des unités choisit la largeur du cône de détection ToF (`1` = large ~27°, `2` = moyen ~20°, `3` = étroit ~15°, voir « Limites connues ») et avance d'un cran à chaque relâchement du **bouton B**. Les deux réglages sont indépendants et peuvent être changés dans n'importe quel ordre avant la validation. Sans activité pendant `MENU_TIMEOUT_MS` (4 s), le menu se referme (bip neutre), les deux choix sont enregistrés en EEPROM, et une nouvelle calibration démarre automatiquement si le capteur a changé, ou si la largeur ROI a changé et que le ToF est le capteur actif. Pendant que le menu est ouvert, le comptage est entièrement suspendu et reprend exactement où il en était à la sortie.
- **Remise à zéro** : il faut maintenir les **deux boutons** enfoncés ensemble pendant `RESET_HOLD_MS` (5 s) — motif sonore dédié (deux notes graves montantes) à la fin. Pendant l'attente, l'afficheur montre un décompte en secondes (`5`, `4`, `3`, `2`, `1`) à la place du compteur, pour savoir où on en est et pouvoir relâcher avant la remise à zéro si c'était involontaire. Ce geste volontairement plus engageant qu'un simple appui remplace l'ancien reset sur simple pression de D3, jugé trop facile à déclencher par accident.
- **Aperçu distance** : en usage normal (hors menu), un appui court sur l'un ou l'autre bouton (relâché avant `LONG_PRESS_MS`) affiche pendant `PEEK_DURATION_MS` (~1,2 s) la distance instantanée mesurée par le capteur actif (`----` si aucune lecture valide) et allume le laser de pointage pour le même intervalle — pratique pour vérifier l'alignement après l'installation sans relancer une calibration — sans interrompre le comptage en arrière-plan.
- **Mode silencieux** : au démarrage, si les deux boutons sont maintenus appuyés simultanément (avant ou juste après la mise sous tension), le mode silencieux bascule (activé ↔ désactivé). L'afficheur clignote 3 fois pour confirmer : tous segments allumés (`8888`) signale mode muté, tous segments éteints (`0`) signale mode non muté. Ce réglage persiste en EEPROM. Lorsque le mode est actif, tous les bips sont désactivés (comptage, calibration, menu, reset, blocage), mais le compteur et toutes les fonctionnalités continuent de fonctionner normalement. Utile dans les environnements où le silence est requis.
- **Contrôle du volume** : le paramètre `BUZZER_VOLUME_PERCENT` (0-100%, défaut 50) dans `src/config.h` réduit la durée des bips pour les rendre **perçus** plus discrets (ce n'est pas un vrai contrôle d'amplitude). À 50%, les bips durent la moitié du temps (17,5 ms au lieu de 35 ms), ce qui les rend moins intrusifs sans compromettre leur audibilité. En dessous de 30-40%, ils risquent d'être trop courts pour être utiles. Ce réglage ne persiste pas en EEPROM et nécessite une recompilation ; il complète le mode silencieux pour un contrôle gradué entre « plein volume » et « muet ».
- **Sauvegarde** : le compteur, le capteur sélectionné et le mode silencieux sont conservés en EEPROM après une coupure ou un redémarrage. La remise à zéro est également enregistrée. Les écritures tournent sur ~113 emplacements de l'EEPROM (au lieu d'une adresse fixe) pour répartir l'usure ; au démarrage, le firmware relit tous les emplacements pour retrouver le plus récent valide.

## Réglages

Tout est regroupé en haut de `src/main.cpp` :

| Constante | Rôle | Défaut |
|---|---|---|
| `MARGIN_CM` | marge sous la distance au mur pour détecter | 20 |
| `CONFIRM_READS` | lectures concordantes requises pour une arrivée | 2 |
| `CLEAR_HOLD_MS` | vide continu requis avant de pouvoir compter un nouveau passage | 500 |
| `MAX_PRESENCE_MS` | délai avant l'état « bloqué » | 10000 |
| `LOOP_DELAY_MS` | pause entre deux mesures | 40 |
| `CALIBRATION_BEEP_HZ` | fréquence du bip de fin de calibration | 1600 |
| `COUNT_BEEP_HZ` | fréquence du bip de confirmation | 2200 |
| `BLOCKED_BEEP_FIRST_HZ` | fréquence de la première note de blocage | 1100 |
| `BLOCKED_BEEP_SECOND_HZ` | fréquence de la seconde note de blocage | 700 |
| `BEEP_DURATION_MS` | durée de chaque note (ms) | 35 |
| `BUZZER_VOLUME_PERCENT` | volume du buzzer (0-100%), réduit la durée des bips | 50 |
| `CALIBRATION_MIN_MS` | durée minimale de l'animation de calibration | 2000 |
| `CALIBRATION_TIMEOUT_MS` | abandon de la calibration sans lecture valide | 10000 |
| `SNAKE_STEP_MS` | vitesse du serpent (ms par image) | 80 |
| `EEPROM_SLOT_COUNT` | nombre d'emplacements de l'anneau EEPROM (voir plus bas) | ~128 |
| `EEPROM_MAGIC` | signature validant le contenu EEPROM | 0x5046 |
| `LONG_PRESS_MS` | appui seul tenu pour ouvrir le menu | 1200 |
| `RESET_HOLD_MS` | les deux boutons tenus pour remettre à zéro (décompte affiché) | 5000 |
| `MENU_TIMEOUT_MS` | inactivité dans le menu avant validation/sortie | 4000 |
| `MENU_BLINK_MS` | clignotement de l'afficheur en mode menu | 300 |
| `PEEK_DURATION_MS` | durée d'affichage de l'aperçu distance | 1200 |
| `MENU_ENTER_BEEP_FIRST_HZ` / `MENU_ENTER_BEEP_SECOND_HZ` | notes d'entrée dans le menu | 1800 / 2400 |
| `MENU_EXIT_BEEP_HZ` | note de sortie du menu | 1500 |
| `RESET_BEEP_FIRST_HZ` / `RESET_BEEP_SECOND_HZ` | notes de confirmation du reset | 500 / 900 |
| `CLICK_BEEP_HZ` / `CLICK_DURATION_MS` | clic de basculement menu / aperçu distance | 3000 / 15 |
| `TOF_TIMING_BUDGET_US` | budget de mesure ToF (µs) | 50000 |

## Entrées vs passages

L'affichage montre le nombre brut de passages. Si chaque visiteur entre puis ressort, les entrées valent environ `passages / 2` : remplace `passCount` par `passCount / 2` dans les deux appels d'affichage. Le brut a l'avantage de révéler qu'une personne est encore à l'intérieur quand le chiffre est impair.

## Limites connues

- Le capteur est placé à environ 1 m du sol et vise un mur à moins de 1,5 m : au-delà, l'absence d'écho se confond avec un passage.
- Deux personnes côte à côte sont comptées une seule fois.
- Les vêtements épais absorbent l'écho : la présence est alors maintenue plus longtemps que le passage réel, ce qui peut finir par déclencher l'état « bloqué ».
- Chaque passage compté déclenche une écriture EEPROM. Grâce à la rotation sur ~113 emplacements (wear-leveling), l'endurance effective passe d'environ 100 000 à environ 11,3 millions d'écritures au lieu de réutiliser toujours la même adresse.
- Réduire la largeur ROI du ToF (menu, niveau « moyen » ou « étroit ») diminue le cône de détection pour limiter le risque de double-détection latérale, mais réduit aussi la quantité de lumière réfléchie captée : portée utile plus courte et mesures plus bruitées, surtout combiné au mode de distance `Long` déjà actif dans le firmware.
- Le `TOF_TIMING_BUDGET_US` contrôle le temps d'intégration du capteur ToF : plus il est élevé, meilleure est la portée et plus le signal est propre, mais le taux de mesure diminue. La valeur par défaut de 50 ms maintient un bon taux d'échantillonnage (~20 Hz) mais peut nécessiter d'être augmentée en conditions difficiles. Si le capteur retourne systématiquement `range_status=7` (signal trop faible), augmenter ce réglage à 100000 (100 ms) ou 140000 (140 ms), ou vérifier les conditions d'installation (distance au mur, réflectivité, lumière ambiante).
- L'afficheur n'a que 4 chiffres : au-delà de 9999 passages l'affichage n'est plus lisible, même si le compteur interne monte jusqu'à 65535.
- Si le mode ToF est sélectionné dans le menu alors que le VL53L1X n'est pas branché (ou n'a pas répondu au démarrage), la calibration échoue après `CALIBRATION_TIMEOUT_MS` (10 s) et le seuil précédent est conservé. Il faut rebrancher le capteur ou rebasculer sur le mode ultrason via le menu.
- Le menu et l'aperçu distance utilisent les mêmes deux boutons que le reset : un relâchement lors d'une tentative de reset abandonnée (un bouton relâché avant les 2 s) peut déclencher un aperçu distance furtif sans conséquence.
- Le laser de pointage ne s'allume que pendant la calibration et l'aperçu distance ; si le menu est ouvert pendant qu'un aperçu distance est encore actif, le laser peut rester allumé un peu plus longtemps que `PEEK_DURATION_MS` jusqu'à la fermeture du menu (cas rare, sans conséquence).
- Le mode silencieux nécessite de maintenir les deux boutons appuyés dès la mise sous tension : le système attend 1 seconde avant de vérifier l'état des boutons, donc ils doivent être maintenus pendant au moins cette durée. Une fois le mode activé, il persiste en EEPROM et reste actif jusqu'au prochain toggle manuel.
- Cette mise à jour change le format d'enregistrement EEPROM (nouveau `EEPROM_MAGIC = 0x5047`, ajout du champ mode silencieux) : au premier flash, le compteur repart une fois à 0, comme lors des précédents changements de format. Le nombre d'emplacements EEPROM passe de ~128 à ~113, mais l'endurance reste excellente (~11,3 millions d'écritures).
