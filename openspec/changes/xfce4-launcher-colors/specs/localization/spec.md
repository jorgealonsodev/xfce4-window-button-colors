# Localization Specification

## Purpose

Defines gettext-based translation coverage for user-facing strings, shipped
for Spanish and English in v1 and structured for future languages.

## Requirements

### Requirement: Spanish and English Catalogs (RF-11)

The system MUST ship gettext `.po`/`.mo` catalogs for Spanish and English
covering every user-facing string (menu items, dialog labels, CLI help text).

#### Scenario: UI strings render in the session's language

- GIVEN the user's session locale is `es_ES` or `en_US`
- WHEN a context-menu item or dialog is displayed
- THEN its text appears in the matching language's catalog
- Manual verification only: requires a live session with the target locale set.

#### Scenario: Catalog covers every marked string

- GIVEN the extracted `.pot` template
- WHEN the Spanish and English `.po` files are checked against it
- THEN both catalogs provide a translation for every extracted string, with no missing entries
- Unit-testable under Meson: yes, via a completeness check (e.g. `msgfmt --statistics`) integrated into the build or test suite.

### Requirement: Extensibility for Additional Languages

The system MUST structure its translation catalogs so that adding a new
language requires only a new `.po` file, with no source code changes.

#### Scenario: A new locale is picked up without a logic change

- GIVEN a new `.po` file is compiled into a locale directory recognized by the module's gettext domain
- WHEN the module runs under that locale
- THEN its UI strings render in the new language with no change to C source
- Partially unit-testable under Meson: catalog structure and domain binding
  are testable; full runtime locale switching requires manual verification.
