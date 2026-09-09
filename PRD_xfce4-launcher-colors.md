# PRD — xfce4-launcher-colors

**Versión:** 1.1 · **Fecha:** 2026-09-09 · **Estado:** Validado contra fuentes upstream · pendiente spike H0

> **Cambios v1.1.** Se corrigen tres puntos técnicos tras verificar el código fuente de `xfce4-panel` (etiquetas 4.16.0, 4.18.0 y `master`): el mecanismo de inserción en el menú (§7.3), el selector CSS del ejemplo (§7.3) y la fecha de migración a GTK4 (§11). Se reescribe §7.4 separando lo verificado de lo pendiente, con citas. Se reestima el Hito 0 (§12).

---

## 1. Resumen

Herramienta para Linux/XFCE que añade al menú contextual (clic derecho) de cada lanzador del panel una opción **«Color de fondo…»**. Al elegir un color, el icono muestra ese color como fondo de forma persistente, permitiendo distinguir visualmente lanzadores idénticos (por ejemplo, varias instancias de VS Code o varias terminales con perfiles distintos, como en la captura de referencia).

Se distribuye como paquete `.deb` (Debian/Ubuntu/Mint/MX) y como `tar.gz` con script de instalación para el resto de distribuciones.

## 1.1 Nombre e identidad

| Elemento | Valor |
|----------|-------|
| Nombre del proyecto / repositorio | `xfce4-launcher-colors` |
| Nombre del paquete (.deb, AUR, spec) | `xfce4-launcher-colors` |
| Binario CLI | `xfce4-launcher-colors` |
| Módulo GTK | `libxfce4-launcher-colors.so` |
| Directorio de configuración | `~/.config/xfce4-launcher-colors/` |
| Nombre visible en la interfaz | «Colores de lanzadores» (título de diálogos); ítems de menú «Color de fondo…» / «Quitar color» |

El nombre sigue la convención de XFCE (`xfce4-<componente>-<función>`) y deja claro que actúa únicamente sobre lanzadores, no sobre otros plugins del panel.

## 2. Problema

XFCE permite colocar varios lanzadores con el mismo icono en el panel, pero no ofrece ninguna forma nativa de diferenciarlos. Hoy la única solución es editar a mano `~/.config/gtk-3.0/gtk.css` conociendo el id interno del plugin, algo inaccesible para la mayoría de usuarios.

## 3. Objetivos y no-objetivos

**Objetivos**
- Colorear el fondo de cualquier lanzador del panel desde su menú contextual, en menos de 3 clics.
- Consumo de recursos prácticamente nulo: **cero procesos adicionales** en ejecución, sin demonio.
- Persistencia entre sesiones, con opción del usuario para activar/desactivar la carga al iniciar sesión.
- Instalación en un paso (`.deb`) o script (`tar.gz`).

**No-objetivos (v1)**
- Colorear otros plugins que no sean lanzadores (reloj, bandeja, etc.).
- Soporte para paneles que no sean xfce4-panel (LXQt, MATE…).
- Cambiar el icono en sí, solo el fondo.
- Soporte Wayland-específico (funciona igual, pero no se prueba activamente en v1).

## 4. Usuarios

- Desarrolladores y usuarios avanzados de XFCE con varios lanzadores similares.
- Usuarios de distros ligeras (Xubuntu, Linux Mint XFCE, MX, Debian XFCE, Arch/EndeavourOS, Fedora XFCE Spin).

## 5. Requisitos funcionales

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| RF-1 | Al hacer clic derecho sobre un lanzador del panel aparece el ítem **«Color de fondo…»** integrado en el menú nativo, por encima de «Propiedades». | Must |
| RF-2 | Al pulsarlo se abre el selector de color estándar de GTK (`GtkColorChooserDialog`) con paleta rápida + editor libre con transparencia (alfa). | Must |
| RF-3 | Al aceptar, el fondo del lanzador cambia **al instante**, sin reiniciar el panel. | Must |
| RF-4 | El color se guarda y se restaura en cada inicio de sesión. | Must |
| RF-5 | Ítem **«Quitar color»** (solo visible si el lanzador tiene color) que restaura el aspecto original. Nota de implementación: el menú del plugin se construye una sola vez y se cachea, así que la visibilidad condicional se resuelve con `gtk_widget_set_no_show_all()` + `gtk_widget_set_visible()` sobre el ítem ya insertado, nunca reinsertándolo (ver §7.4). | Must |
| RF-6 | Opción **«Cargar al iniciar sesión»** (casilla en el submenú o en un mini-diálogo de ajustes). Activa por defecto tras instalar. Si está desactivada, el módulo no se carga y los colores no se aplican hasta activarla de nuevo. | Must |
| RF-7 | Ajustes de estilo: radio de esquinas (0–12 px) y margen interior, aplicables globalmente. | Should |
| RF-8 | Al eliminar un lanzador del panel, su color se limpia del fichero de configuración (sin dejar basura). | Should |
| RF-9 | El color se conserva al mover el lanzador de posición o de panel (el id del plugin no cambia). | Must |
| RF-10 | Comando CLI `xfce4-launcher-colors set <id> <#rrggbbaa>` / `unset <id>` / `list` para scripting y depuración. | Could |
| RF-11 | Traducción: español e inglés en v1 (gettext), preparado para más idiomas. | Should |

## 6. Requisitos no funcionales

| ID | Requisito |
|----|-----------|
| RNF-1 | **Memoria:** incremento < 1 MB de RSS en el proceso `xfce4-panel`. Sin procesos residentes propios. |
| RNF-2 | **CPU:** cero uso en reposo; el módulo solo actúa al abrir un menú contextual o al cargarse. |
| RNF-3 | **Arranque:** carga del módulo y aplicación de CSS < 10 ms. |
| RNF-4 | **Dependencias mínimas:** solo bibliotecas que XFCE ya tiene instaladas (GTK3, libxfce4panel, libxfce4util, xfconf). Sin Python, sin Electron, sin Qt. |
| RNF-5 | **Tamaño:** paquete `.deb` < 100 KB. |
| RNF-6 | **Robustez:** si el módulo falla al cargar, el panel sigue funcionando con normalidad (el módulo debe abortar en silencio, nunca provocar un crash). |
| RNF-7 | **Compatibilidad:** xfce4-panel 4.16, 4.18 y 4.20 (todos GTK3). |
| RNF-8 | Sin modificar ficheros del sistema ni del propio XFCE; todo en `$XDG_CONFIG_HOME`. |
| RNF-9 | Licencia GPL-2.0-or-later (compatible con libxfce4panel). |

## 7. Decisión tecnológica

### 7.1 Opciones evaluadas

| Opción | Recursos | Integración en menú nativo | Funciona con lanzadores existentes | Veredicto |
|--------|----------|----------------------------|-------------------------------------|-----------|
| **A. Módulo GTK en C cargado en xfce4-panel** | Excelente (in-process, <1 MB) | Sí | Sí | **Elegida** |
| B. Plugin de panel nuevo («Lanzador con color») | Excelente | Sí (menú propio) | No: el usuario debe recrear sus lanzadores | Plan B |
| C. Demonio Python/GTK escuchando eventos | Malo (30–60 MB residentes) | No (no puede tocar el menú del panel) | Parcial | Descartada |
| D. Fork parcheado de xfce4-panel | Excelente | Sí | Sí | Descartada: inmantenible, choca con el paquete de la distro |
| E. Solo script que edita `gtk.css` | Excelente | No (sin menú contextual) | Sí | Descartada: no cumple RF-1 |

### 7.2 Stack elegido

- **Lenguaje:** C (C11).
- **Toolkit:** GTK 3 (el que usa xfce4-panel actualmente).
- **Bibliotecas:** `libxfce4panel-2.0`, `libxfce4util-1.0`, `xfconf-0`.
- **Build:** Meson + Ninja (más rápido y ligero que autotools; es el estándar actual en XFCE).
- **Persistencia:** fichero CSS propio `~/.config/xfce4-launcher-colors/colors.css` + `settings.ini` (radio, margen).
- **i18n:** gettext.

### 7.3 Cómo funciona (arquitectura)

```
xfce4-panel (proceso existente)
 └── carga libxfce4-launcher-colors.so  (módulo GTK, vía XSETTINGS Gtk/Modules)
      ├── comprueba g_get_prgname() == "xfce4-panel"; en otro proceso → no hace nada y sale
      ├── al cargar: GtkCssProvider con colors.css → gtk_style_context_add_provider_for_screen()
      ├── registra UN emission hook global: g_signal_add_emission_hook(g_signal_lookup("map",
      │    GTK_TYPE_WIDGET), ...) para detectar cada lanzador cuando aparece
      │    └── el callback filtra por XFCE_IS_PANEL_PLUGIN + prefijo de nombre "launcher-"
      ├── por cada lanzador detectado, ANTES de su primer clic derecho:
      │    xfce_panel_plugin_menu_insert_item() × 2 → "Color de fondo…" / "Quitar color"
      └── al elegir color: reescribe colors.css y recarga el provider (efecto inmediato)

colors.css (ejemplo generado)
  #launcher-13 #launcher-arrow { background-color: rgba(233,30,140,0.85); border-radius: 6px; }
  #launcher-21 #launcher-arrow { background-color: #3daee9; border-radius: 6px; }
```

> **Por qué el selector lleva dos niveles.** El contenedor externo (`#launcher-13`) es único por
> instancia, pero el widget que realmente pinta y recibe el hover es el botón interno, cuyo nombre
> efectivo en tiempo de ejecución es **`launcher-arrow`** y es idéntico en todos los lanzadores
> (ver §7.4, punto 3). El selector descendente combina ambos: el ancestro aporta la unicidad y el
> descendiente aporta el widget correcto. Así se conserva un único fichero `colors.css` global con
> un solo `GtkCssProvider`, sin necesidad de clases dinámicas ni de un provider por widget.

Puntos clave:
- **Identificación del lanzador:** xfce4-panel asigna a cada plugin el nombre de widget `<tipo>-<id>` (p. ej. `launcher-13`), estable mientras el plugin exista. Ese nombre es el ancestro del selector CSS. El id se obtiene con `xfce_panel_plugin_get_unique_id()`, que devuelve `gint`.
- **Inserción en el menú:** se usa la API pública `xfce_panel_plugin_menu_insert_item()`, no la intercepción de señales. Es la vía que usa en producción `xfce4-whiskermenu-plugin` y está presente sin cambios desde 4.16.0.
- **Carga del módulo:** se registra en la clave xfconf `/Gtk/Modules` del canal `xsettings` (misma técnica que usa `appmenu-gtk-module`). xfsettingsd la propaga por XSETTINGS y GTK carga el módulo en cada app GTK3; el módulo se autodescarta salvo en `xfce4-panel`, por lo que el coste en el resto de apps es una comprobación de nombre. Esto **es** el mecanismo de «cargar al iniciar sesión»: la casilla del RF-6 añade o quita el módulo de esa clave.
- **Aplicación inmediata:** al estar dentro del proceso del panel, basta con recargar el `GtkCssProvider`; no hay que reiniciar nada.

### 7.4 Supuestos: estado de validación

Los supuestos que sostenían la elección de la opción A se verificaron leyendo el código fuente de
`xfce4-panel` en las etiquetas `xfce4-panel-4.16.0`, `xfce4-panel-4.18.0` y `master`. **Ninguna
evidencia obliga a activar el Plan B.**

#### Verificado

**1. Los lanzadores corren dentro del proceso del panel.** `plugins/launcher/launcher.desktop.in`
declara `X-XFCE-Internal=TRUE`, y `panel/panel-module.c` ejecuta esos plugins in-process en X11 sin
proceso envoltorio. Un módulo GTK cargado en `xfce4-panel` sí ve sus widgets.
*Limitación:* la garantía encontrada en el código es específica de X11. El comportamiento en Wayland
no está confirmado, lo cual es coherente con el no-objetivo de v1.

**2. El nombre de widget `launcher-<id>` existe y es estable.** En
`libxfce4panel/xfce-panel-plugin.c`, `xfce_panel_plugin_set_property()` ejecuta
`g_strdup_printf("%s-%d", priv->name, priv->unique_id)` seguido de `gtk_widget_set_name()` para todo
plugin.

**3. El nombre efectivo del botón clicable es `launcher-arrow`, no `launcher-button`.** Éste es el
hallazgo que más impacto tiene sobre el diseño. `launcher_plugin_init()` asigna primero
`gtk_widget_set_name(plugin->button, "launcher-button")` y más abajo, en el bloque que construye la
flecha, ejecuta `gtk_widget_set_name(plugin->button, "launcher-arrow")` — apuntando de nuevo a
`plugin->button` en lugar de a `plugin->arrow`. Como `gtk_widget_set_name()` sobrescribe, el nombre
final del botón es `launcher-arrow`, y `plugin->arrow` se queda sin nombre. El propio panel lo
confirma: su regla interna de tamaño mínimo usa el selector `#launcher-arrow` aplicado al contexto
de estilo de `plugin->button`.

Es un error del upstream, pero es idéntico en 4.16.0, 4.18.0 y `master`, así que a efectos prácticos
es un contrato estable. **Un selector `#launcher-button` no coincidiría con nada y fallaría en
silencio.** Debe documentarse en el código, porque es contraintuitivo para cualquiera que lo lea
después.

**4. La API pública de menú existe desde 4.16.** `xfce_panel_plugin_menu_insert_item()` y
`xfce_panel_plugin_menu_destroy()` están declaradas en el header de la etiqueta 4.16.0 y no llevan
anotación `Since:`, o sea que son anteriores. No hay riesgo de regresión de versión y el RNF-7 se
mantiene. Sustituyen a los candidatos originales (`popup-menu` / `button-press-event`), que eran
menos estables entre versiones.

**5. `xfsettingsd` propaga `/Gtk/Modules`.** Reenvía cualquier clave `/Gtk/*` a XSETTINGS sin lista
blanca que excluya `Modules`. El precedente de `appmenu-gtk-module` es real. GTK3 documenta
`gtk-modules` vía XSETTINGS como uno de sus tres mecanismos estándar de carga.

**6. Prioridades de provider CSS.** `THEME` = 200, `APPLICATION` = 600, `USER` = 800.

#### Restricciones descubiertas que el diseño debe respetar

**El menú del plugin se construye una sola vez y se cachea.** En `xfce_panel_plugin_menu_get()`, el
bucle que añade los ítems personalizados vive *dentro* del bloque `if (plugin->priv->menu == NULL)`.
Consecuencias:

- Insertar un ítem **después** de que el menú se haya construido no tiene ningún efecto visible.
  Por eso el módulo debe insertar sus ítems en cuanto detecta el lanzador, antes del primer clic
  derecho del usuario.
- `xfce_panel_plugin_menu_destroy()` no es una salida cómoda: libera **todos** los ítems
  personalizados del plugin, no solo los nuestros, y además es un no-op mientras el menú esté
  visible. Usarlo para refrescar nuestro propio ítem rompería los de cualquier otro módulo.
- La visibilidad condicional del RF-5 («Quitar color» solo si hay color) se resuelve, por tanto,
  con `gtk_widget_set_no_show_all(item, TRUE)` y `gtk_widget_set_visible()` sobre el ítem ya
  insertado, actualizado justo antes de cada popup.

**El nombre del botón interno es compartido.** `launcher-arrow` es idéntico en todos los lanzadores,
así que no sirve por sí solo para distinguir instancias. Se resuelve con el selector descendente
`#launcher-<id> #launcher-arrow` (§7.3), que aprovecha la unicidad del ancestro.

#### Pendiente de confirmar en el spike (Hito 0)

1. **Prioridad de provider a usar.** El panel añade su propia regla sobre `#launcher-arrow` a nivel
   de widget con prioridad `APPLICATION` (600). Un provider nuestro a nivel de pantalla con la misma
   prioridad podría perder, porque los providers de widget tienen precedencia sobre los de pantalla.
   `USER` (800) ganaría, pero empata con el `gtk.css` del usuario y le quita la última palabra.
   Decisión a tomar con la evidencia del spike, no antes.
2. **Que el fondo se pinte de verdad.** Que el selector coincida no garantiza que se vea: hay que
   confirmar contra temas reales (Adwaita y el tema por defecto de Xubuntu) que el color no queda
   tapado y que el hover sigue funcionando.
3. **Elección del hook de creación.** `g_signal_add_emission_hook()` sobre `GtkWidget::map` es la
   opción recomendada: es global, se registra una sola vez y no hace polling, lo que satisface el
   RNF-2. Falta confirmar en ejecución que `map` es el momento correcto para poder recorrer los
   ancestros y encontrar el `XfcePanelPlugin`, y medir el coste del filtrado, dado que el hook se
   dispara para *todos* los widgets del proceso.
4. **Sin divergencias de la distro.** Comprobar el header instalado
   (`/usr/include/xfce4/libxfce4panel-2.0/…`) por si la distro aplica parches sobre el upstream.
5. **Caché de menú en 4.16/4.18.** La ruta de código del cacheo se verificó en 4.18.0 y `master`;
   conviene confirmarla también en 4.16.0.

#### Nota de procedencia

Las citas de código se obtuvieron de los mirrors de GitHub de `gitlab.xfce.org`
(`xfce-mirror/xfce4-panel`), porque el navegador de ficheros de GitLab no se puede recuperar de
forma automatizada. Conviene reconfirmarlas contra la instancia canónica cuando se instalen las
cabeceras de desarrollo.

## 8. Interfaz de usuario

**Menú contextual del lanzador (con módulo activo):**
```
Color de fondo…            ← RF-1
Quitar color               ← solo si tiene color (RF-5)
xfce4-launcher-colors ▸
    ☑ Cargar al iniciar sesión   ← RF-6
    Ajustes de estilo…           ← RF-7
────────────────
Propiedades
Acerca de
Mover
Eliminar
…
```

**Diálogo de color:** el `GtkColorChooserDialog` nativo, con 9 colores sugeridos por defecto (rojo, naranja, amarillo, verde, cian, azul, violeta, magenta, gris) y botón «Personalizado» para RGBA libre. Botones: Cancelar / Aplicar.

**Diálogo de ajustes (pequeño, 2 controles):** radio de esquinas (deslizador), margen (deslizador), botón «Restablecer todos los colores».

## 9. Empaquetado y distribución

### 9.1 `.deb`
- Nombre: `xfce4-launcher-colors`, arquitecturas `amd64` y `arm64`.
- `Depends: libgtk-3-0, libxfce4panel-2.0-4, libxfce4util7, libxfconf-0-3, xfce4-panel (>= 4.16)`.
- Ficheros:
  - `/usr/lib/<triplet>/gtk-3.0/modules/libxfce4-launcher-colors.so`
  - `/usr/bin/xfce4-launcher-colors` (CLI, RF-10)
  - `/usr/share/applications/xfce4-launcher-colors.desktop` (`NoDisplay=true`, para ajustes)
  - `/usr/share/locale/…`
- `postinst`: no toca la configuración del usuario. La activación es por usuario y ocurre en el primer uso o mediante `xfce4-launcher-colors --enable` (que escribe la clave xsettings y recarga el panel con `xfce4-panel -r`).
- Firmado y apto para un PPA / repositorio propio; `debian/` compatible con `dpkg-buildpackage`.
- Estándar Debian policy, lintian limpio.

### 9.2 `tar.gz` (otras distros)
- `xfce4-launcher-colors-<versión>.tar.gz` con fuentes, `meson.build`, `README.md` e `install.sh`.
- `install.sh`: detecta el gestor de paquetes (pacman, dnf, zypper, apk, xbps) e imprime el comando para instalar dependencias de compilación; luego `meson setup build && ninja -C build && sudo ninja -C build install`. Opción `--user` para instalar en `~/.local` sin root.
- Se aportarán como cortesía un `PKGBUILD` (AUR) y un `.spec` (Fedora/openSUSE), aunque no se mantienen como entregables oficiales.

## 10. Opción «cargar al iniciar sesión»

| Estado | Comportamiento |
|--------|----------------|
| Activada (por defecto tras `--enable`) | El módulo está en `/Gtk/Modules`; xfce4-panel lo carga en cada sesión y aplica `colors.css`. |
| Desactivada | Se retira de `/Gtk/Modules`. El panel arranca limpio; los colores quedan guardados pero no visibles. El menú contextual vuelve a ser el nativo. Se puede reactivar con el CLI `--enable` o desde el diálogo de ajustes lanzado desde el menú de aplicaciones. |

No se usa `~/.config/autostart` porque no hay ningún proceso que arrancar: la «autocarga» es la carga del módulo dentro del panel.

## 11. Riesgos

| Riesgo | Impacto | Mitigación |
|--------|---------|------------|
| xfce4-panel migra a GTK4 (**sin fecha confirmada**) | Alto | Ninguna fuente upstream respalda la fecha «4.22+» que figuraba en la v1.0 de este PRD: 4.20.x sigue en GTK 3.24 y la prioridad declarada de 4.20 fue Wayland/Layer-Shell, no GTK4. El módulo se compilará contra GTK4 cuando llegue; la persistencia por CSS sigue siendo válida. Seguir la rama de desarrollo de XFCE. |
| El punto de enganche al menú cambia entre versiones | Bajo | Mitigado: se usa la API pública `xfce_panel_plugin_menu_insert_item()`, presente sin cambios desde 4.16.0 (§7.4). Tests en 4.16/4.18/4.20 en CI. Plan B sigue disponible pero ninguna evidencia lo exige. |
| `/Gtk/Modules` carga el `.so` en todas las apps GTK3 | Bajo | Salida inmediata si no es `xfce4-panel` (coste: microsegundos). Documentarlo. |
| Temas GTK que sobrescriben el fondo de botones del panel | Medio | Selector descendente `#launcher-<id> #launcher-arrow` (§7.3). La prioridad de provider se decide en el H0: el panel ya añade una regla propia a nivel de widget con prioridad `APPLICATION`, y los providers de widget ganan a los de pantalla a igual prioridad. |
| El selector CSS no coincide y no se pinta nada, en silencio | Alto | Causa concreta: usar `#launcher-button`, que es el nombre que el panel asigna y luego sobrescribe. El nombre efectivo es `launcher-arrow` (§7.4). Documentarlo en el código y cubrirlo con un test de generación de CSS. |
| Insertar el ítem de menú demasiado tarde y que no aparezca | Medio | El menú se construye una vez y se cachea; el ítem debe insertarse al detectar el lanzador, no al abrir el menú. No usar `menu_destroy()` para refrescar: libera también los ítems de otros módulos (§7.4). |
| Distros con xfce4-panel < 4.16 | Bajo | No soportadas; el módulo lo detecta y avisa una vez. |

## 12. Hitos

| Hito | Contenido | Estimación |
|------|-----------|------------|
| H0 — Spike técnico | La decisión A vs. B ya está resuelta con evidencia documental (§7.4): se sigue con A. El spike se reenfoca en los cinco puntos pendientes de §7.4, y su parte más incierta es el emission hook sobre `GtkWidget::map` y la prioridad de provider, que solo se resuelven en ejecución. Incluye instalar el toolchain de compilación, que hoy no está presente. | 3–4 días |
| H1 — MVP | RF-1 a RF-5, RF-9. Instalación manual. | 1 semana |
| H2 — Autocarga y ajustes | RF-6, RF-7, RF-8, CLI. | 3–4 días |
| H3 — Empaquetado | `.deb` (amd64/arm64), `tar.gz` + `install.sh`, PKGBUILD/spec, CI en GitHub Actions. | 3–4 días |
| H4 — Pulido | i18n es/en, README con capturas, pruebas en Xubuntu 24.04/26.04, Mint XFCE, Debian 13, Arch, Fedora. | 3 días |

## 13. Criterios de aceptación

1. En Xubuntu 24.04 limpio, `sudo apt install ./xfce4-launcher-colors_1.0_amd64.deb && xfce4-launcher-colors --enable` → clic derecho en un lanzador muestra «Color de fondo…».
2. Elegir un color lo aplica en < 100 ms sin parpadeo del panel.
3. Cerrar sesión y volver a entrar mantiene los colores.
4. Desmarcar «Cargar al iniciar sesión», reiniciar sesión → panel nativo sin colores y sin ítem extra; volver a marcar → todo restaurado.
5. `ps aux` no muestra ningún proceso propio; `smem`/`pmap` sobre `xfce4-panel` muestra < 1 MB atribuible al módulo.
6. En Arch, `tar xzf … && ./install.sh` deja el módulo funcionando.
7. Eliminar un lanzador del panel borra su entrada de `colors.css`.
8. Desinstalar el paquete deja el panel exactamente como estaba (salvo el fichero de configuración del usuario, que se conserva).

## 14. Métricas de éxito (post-lanzamiento)

- ≥ 90 % de instalaciones sin incidencias en las 5 distros probadas.
- 0 informes de crash del panel atribuibles al módulo.
- Tiempo desde clic derecho hasta color aplicado ≤ 3 interacciones.
