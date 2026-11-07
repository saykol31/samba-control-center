# Samba Control Center

Aplicación de escritorio (GTKmm 4 / C++17, binario nativo) para administrar
**Samba en tu propio equipo Linux en los dos sentidos**: como **servidor**
(comparte tus carpetas con la red local, por ejemplo con un equipo Windows)
y como **cliente** (descubre otros equipos y conéctate a sus recursos),
sin tocar archivos de configuración a mano y sin teclear comandos.

Todo se opera desde una sola ventana de seis pestañas, con un registro
(log) que muestra, paso a paso, lo que hizo cada acción y con qué
resultado. Si algo falla, el log dice qué paso falló y por qué; ninguna
acción reporta éxito si un paso intermedio falló.

- **Sin carpeta forzada**: tú eliges qué carpetas compartir (una o
  varias); cada una se convierte en un recurso de `/etc/samba/smb.conf`
  y se valida por separado — la que falte se marca y se excluye, sin
  tumbar el resto.
- **Doble init**: el gestor de servicios se detecta en tiempo de
  ejecución (`/proc/1/comm`). Funciona con **systemd** (`systemctl`) y
  con **SysVinit/OpenRC** (`service`, con respaldo a `/etc/init.d/`).
- **Sin contraseñas guardadas**: la aplicación arranca sin privilegios y
  pide autorización de administrador solo cuando una acción la necesita
  (una por acción). Las contraseñas Samba se teclean al momento de
  usarlas y no se guardan en claro en ningún archivo.
- Probado en MX Linux 23 (KDE, SysVinit) y en Debian 13 (WSL).

## El modelo: una máquina se administra; a las demás se visita

La aplicación tiene dos mitades con una frontera clara:

- **Mitad servidor** (HOME, FILE, CONTROL, el SERVER de USER y
  SETTINGS): administra Samba en **este** equipo — sus carpetas,
  sus usuarios, su configuración. Es la única mitad con
  autoridad, y esa autoridad termina en esta máquina.
- **Mitad cliente** (el CLIENT de USER): visita los servidores
  de los demás. Entra como invitada, con el usuario y la
  contraseña de la otra máquina, monta sus recursos y se va.
  Nunca crea recursos ni cambia nada en un equipo ajeno: un
  recurso compartido nace siempre en la máquina dueña de la
  carpeta, y es allí donde se crea.

### Cómo conecta el cliente

1. **SCAN** — *¿quién hay en la red?* Barre tus subredes y
   lista **todas** las máquinas vivas (también teléfonos e
   impresoras). No lee el campo Server: ese campo es el
   *destinatario* de las demás acciones y se rellena al pulsar
   una fila del escaneo, tecleando la IP o el nombre a mano, o
   cargando una conexión guardada.
2. **LIST SHARES** — *¿qué ofreces?* Pregunta al equipo
   elegido qué recursos comparte, con tus credenciales de ese
   equipo. La que responde con recursos, esa es un servidor.
3. **CONNECT** — monta el recurso elegido en tu sesión, sin
   privilegios (vía GVfs, desde la propia aplicación). La
   contraseña se teclea al momento y nunca se guarda; las
   conexiones guardadas recuerdan servidor y recurso, jamás
   la contraseña.

Los dos escaneos de la aplicación no son el mismo. El del
cliente corre **sin privilegios**, como corresponde a
explorar, y por eso es el más lento: pregunta también por los
puertos SMB (139/445) porque un equipo Windows ignora el
ping y, sin esa sonda, no se le vería. El **SCAN NETWORK** de
CONTROL pide autorización de administrador una vez y descubre
por ARP: al instante y con MAC y fabricante — esa tabla
completa solo es posible con privilegio.

### SETTINGS: la memoria, no las manos

Las demás pestañas operan; SETTINGS recuerda. Ahí viven los
valores permanentes, guardados en la configuración de la
aplicación: el **grupo de trabajo** (que al aplicarse se
escribe en el `/etc/samba/smb.conf` de este equipo), la **IP
del equipo Windows de referencia** (la que CONTROL usa en sus
pruebas; **PICK** la elige desde un escaneo en vez de
teclearla) y la tarjeta **DEPENDENCIES**. Los campos de las
demás pestañas — el servidor del cliente, el usuario, el
recurso elegido — son campos de trabajo: cambian en cada uso
y no se guardan.

## Las pestañas

| Pestaña | Qué contiene |
|---|---|
| **HOME** | El estado de un vistazo: servicios `smbd`/`nmbd`, carpeta en uso, red y firewall, con los controles para iniciar, reiniciar y apagar los servicios Samba. |
| **FILE** | Tus carpetas compartidas: elegir una carpeta existente o crearla, darle nombre de recurso y decidir si se publica. Son las carpetas de **este** equipo: FILE no crea recursos en otros equipos. |
| **CONTROL** | Información y diagnóstico del servidor y de la red (ver la tabla de botones). Casi todo aquí *mira*; la única acción grande es configurar Samba. |
| **USER** | A la izquierda, **SERVER**: los usuarios Samba (crear, poner contraseña, borrar, verificar). A la derecha, **CLIENT**: buscar equipos en la red, listar sus recursos con tus credenciales, conectar, abrir y desconectar; las conexiones se guardan **sin** la contraseña. |
| **SETTINGS** | Los valores permanentes (ver «SETTINGS: la memoria, no las manos»): grupo de trabajo del servidor e IP del equipo Windows de referencia (con **PICK**, que escanea la red y te deja elegirla en vez de teclearla). Aquí vive también **DEPENDENCIES**: comprueba cada herramienta que la aplicación necesita y permite instalar las que falten. |
| **HELP** | La guía de uso dentro de la propia aplicación, pestaña por pestaña. |

### Los botones de CONTROL

| Botón | Qué hace |
|---|---|
| 👏 **SAMBA** | La preconfiguración automática en un clic: crea las carpetas compartidas que falten, deja creado el usuario Samba del usuario Linux actual, genera la configuración con los valores de SETTINGS y las carpetas de FILE, la valida con `testparm` **antes** de instalarla, respalda el `smb.conf` actual, reinicia los servicios y abre el firewall. |
| 🛡 **UFW** | Muestra el estado del firewall UFW y su regla para Samba. |
| 🔍 **SCAN NETWORK** | Descubre los equipos de tu red local (IP, nombre, MAC, fabricante y tipo posible). |
| 🌐 **WINDOWS NETWORK** | La conectividad completa con el equipo Windows en un solo informe: ruta, ping, puerto TCP 445 y puerto TCP 139. |
| ✅ **SMB TEST** | Prueba el protocolo SMB contra el equipo Windows con `smbclient`. |
| 🔌 **PORTS 139/445** | Muestra qué procesos escuchan en los puertos de Samba de este equipo. |
| 📟 **INTERFACES** | El detalle completo de las interfaces de red y las direcciones del equipo. |
| 🔀 **NET & ROUTES** | La tabla de rutas, la ruta hacia internet y la ruta hacia el equipo Windows. |
| ⌨ **TCPDUMP** | Captura breve de tráfico para depurar la conexión. |
| 🩺 **FULL DIAGNOSTIC** | El informe entero en **una sola lectura**: dependencias, versión de Samba, carpeta, servicios, firewall, puertos, red y rutas, puerta de enlace (sección GATEWAY), ping y puerto 445 a Windows, validación de la configuración (`testparm`), usuario Samba. Cada bloque aparece una sola vez. |

## Requisitos

Sistema Debian o derivado, con entorno de escritorio.

## Compilar

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libgtkmm-4.0-dev

cd ~/samba-control-center
cmake -S . -B build
cmake --build build --parallel 2
```

## Ejecutar

```bash
./build/samba-control-center
```

Las herramientas que la aplicación usa en tiempo de ejecución (Samba,
`smbclient`, `tcpdump`, `ufw`, `nmap`, `nmblookup`, etc.) no hace falta
instalarlas a mano: la tarjeta **DEPENDENCIES** de SETTINGS las comprueba
al arrancar y ofrece instalar las que falten.

### Si lo corres en WSL

La imagen de Debian para WSL no trae `konsole`, la terminal donde la
aplicación muestra la instalación de dependencias, ni `pkexec`. Instala
`konsole` a mano una vez, antes de abrir la aplicación:

```bash
sudo apt install konsole
```

Sin ella la aplicación avisa de que falta y no puede abrir la consola de
instalación; y sin `pkexec`, los pasos que necesitan administrador no
podrán ejecutarse en WSL (en un Linux normal, MX incluido, `pkexec`
viene con el sistema).

## Licencia

GPL-3.0. Consulta el archivo `LICENSE`.

## Créditos

Desarrollo: **Esequiel Pena Perez**, con asistencia de **Muse (IA)** en
revisión de código, parches y documentación.
