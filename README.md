
## Prérequis logiciels

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) (avec Docker Compose)
- [PlatformIO](https://platformio.org/) (extension VSCode recommandée)
- [Git](https://git-scm.com/)

---

## 1. Cloner le projet

```bash
git clone <url-du-repo>
cd MAR-2-T-IOT-902
```

---

## 2. Démarrer le backend

```bash
cd backend
récupérer le .env dans notion
```

Le fichier `.env` est prêt à l'emploi. Vérifier que le token est cohérent avec le firmware (voir étape 4) :

```
GATEWAY_TOKENS=dev-token
```

Démarrer l'API et la base de données :

```bash
docker compose up --build -d
```

Appliquer les migrations (créer les tables) :

```bash
docker compose run --rm migrate
```

Vérifier que l'API répond :

```bash
curl http://localhost:8000/api/v1/health
# attendu : {"status":"ok","database":"ok","version":"1.0.0"}
```

---

## 3. Tester l'API dans le navigateur

Ouvrir : **http://localhost:8000/docs**

### Authentification

Cliquer sur **Authorize** (cadenas en haut à droite) → entrer `dev-token` → **Authorize**.

### Tester l'ingestion (POST)

1. Cliquer sur `POST /api/v1/measurements` → **Try it out** → **Execute**
2. Résultat attendu : **201** — mesure acceptée et stockée

### Vérifier que le device est créé

1. Cliquer sur `GET /api/v1/devices` → **Try it out** → **Execute**
2. Résultat attendu : **200** — le device `tbem-lora32-001` apparaît dans la liste

### Tester le rejet de doublon

1. Cliquer à nouveau sur **Execute** (même `message_id`)
2. Résultat attendu : **409** — doublon rejeté

### Tester l'authentification

1. Cliquer sur **Authorize** → changer le token par `mauvais-token`
2. Cliquer sur **Execute**
3. Résultat attendu : **401** — accès refusé

---

## 4. Configurer le firmware

```bash
cd firmware
récupérer le fichier platformio.local.ini dans notion
```

Éditer `platformio.local.ini` et changer les valeurs build_flags (nom wifi, mot de passe, adress ip) :



> Pour trouver les ports COM :
> ```powershell
> Get-WMIObject Win32_SerialPort | Select-Object Name, DeviceID, Description
> ```

> Pour trouver l'IP de ton PC :
> ```powershell
> ipconfig
> ```
> Utiliser l'adresse IPv4 du réseau WiFi partagé (ex: `192.168.1.59`).

> Le Heltec (ESP32) ne supporte que le **WiFi 2.4 GHz**.  
> Si tu utilises un hotspot téléphone, forcer la bande 2.4 GHz dans les paramètres.

---

## 5. Flasher le T-Beam

Brancher le T-Beam sur USB.

Depuis VSCode : **Ctrl+Shift+P** → `Tasks: Run Task` → **Flash T-Beam**

Ou en terminal :

```bash
cd firmware
pio run -e tbeam_main --target upload
```

Ouvrir le moniteur série pour vérifier :

```bash
pio device monitor -e tbeam_main
# ou dans VSCode : Tasks > Monitor T-Beam
```

Sortie attendue :

```
[LoRa]
Payload (108 oct) : {"id":"tbeam-001","fw":"1.0.0","msg":"tbeam-001-12345","seq":5,...}
Envoi           : OK
```

---

## 6. Flasher le Heltec (gateway)

Brancher le Heltec sur USB.  
Fermer le monitor série du Heltec s'il est ouvert (le port COM ne peut pas être utilisé par deux processus en même temps).

Depuis VSCode : **Ctrl+Shift+P** → `Tasks: Run Task` → **Flash Heltec (gateway)**

Ou en terminal :

```bash
pio run -e heltec_gateway --target upload
```

Ouvrir le monitor :

```bash
pio device monitor -e heltec_gateway
# ou dans VSCode : Tasks > Monitor Heltec (gateway)
```

---

## 7. Démarrer le frontend

```bash
cd frontend
récupérer .env dans notion
```

Dans `.env`, vérifier que le token correspond à celui du backend :

```
VITE_API_TOKEN=dev-token
```

Installer les dépendances et lancer :

```bash
npm install
npm run dev
```

Ouvrir : **http://localhost:5173**

Le dashboard affiche :
- Statut de l'API en temps réel
- Carte interactive avec la position de chaque capteur
- Grille des devices avec les dernières mesures (PM2.5, température, batterie)
- Page détail par capteur : graphiques température / PM2.5 / batterie, infos LoRa

> Le frontend se connecte automatiquement à `http://localhost:8000` — le backend doit tourner en même temps.

---

## 8. Vérifier la chaîne complète

Le monitor Heltec doit afficher :

```
[Gateway] Heltec WiFi LoRa 32 V3 — demarrage
[WiFi] Connexion a NomDeTonWiFi..........
[WiFi] Connecte — IP : 192.168.x.x
[NTP] Heure synchronisee : 2026-XX-XXTXX:XX:XXZ
[LoRa] SX1262 pret (868.0 MHz | SF7 | BW125 kHz | CR4/5)
[Gateway] En attente de paquets LoRa du T-Beam...

[LoRa] #1 recu | RSSI: -15.0 dBm | SNR: 12.5 dB
       {"id":"tbeam-001","fw":"1.0.0","msg":"tbeam-001-39838",...}
[API] OK (#1) tbeam-001-39838
       OK envoyes : 1 / 1
```

Vérifier dans le navigateur que les données arrivent :
- **http://localhost:5173** → le device `tbeam-001` apparaît sur la carte et dans la grille
- **http://localhost:8000/docs** → `GET /api/v1/devices` → `measurement_count` s'incrémente

---

## Valeurs RSSI/SNR de référence

| RSSI | Interprétation |
|---|---|
| -10 à -40 dBm | Excellent (cartes côte à côte) |
| -40 à -80 dBm | Bon (quelques dizaines de mètres) |
| -80 à -100 dBm | Correct (centaines de mètres) |
| < -110 dBm | Limite — risque de perte de paquets |

SNR > 10 dB = excellent. SNR < 0 dB = signal bruité mais LoRa peut encore décoder.

---

## Dépannage

| Symptôme | Solution |
|---|---|
| `GET /health` échoue | `docker compose logs db` — postgres démarré ? migrations faites ? |
| API retourne 401 | `GATEWAY_TOKENS` dans `.env` ≠ `GATEWAY_API_TOKEN` dans `platformio.local.ini` |
| Heltec ne se connecte pas au WiFi | Vérifier SSID/mdp, forcer 2.4 GHz sur le hotspot |
| Heltec connecté WiFi mais API 422 | Le capteur de poussière calibre au démarrage (4-5 cycles) — normal |
| `Could not open COMx` | Fermer le monitor série avant de flasher |
| `% must be followed by %` | Le mdp WiFi contient `%` — écrire `%%` dans `platformio.local.ini` |
| LoRa — aucun paquet reçu | Vérifier que le T-Beam est sous tension et que les deux cartes utilisent 868 MHz |

---

## Câblage des capteurs (T-Beam)


```bash
# Logs API en direct
docker compose logs -f api

# Relancer l'API après une modification
docker compose up --build -d

# Appliquer les migrations
docker compose run --rm migrate

# Réinitialiser la base (supprime toutes les données)
docker compose down -v && docker compose up -d && docker compose run --rm migrate

# Lancer les tests automatisés
docker compose exec api pytest -v
```

---

*Epitech — T-IOT-902 — Sensor Sensei*
