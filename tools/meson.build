# harness-app: throwaway, non-panel GTK3 process used to verify the
# module lifecycle offline (tasks.md 3.6). It is built but never
# registered as a meson test() and never installed — it is run manually,
# with the module loaded into it via GTK_MODULES, exactly as documented
# in tools/harness-app.c's own header comment.
harness_app = executable(
  'harness-app',
  'harness-app.c',
  dependencies: [gtk3_dep],
  install: false,
)

# idempotency-probe: directly calls gtk_module_init()/gtk_module_exit()
# twice each in one process, to prove idempotency (tasks.md 3.3) instead
# of inferring it. See tools/idempotency-probe.c's own header comment.
idempotency_probe = executable(
  'idempotency-probe',
  'idempotency-probe.c',
  dependencies: [gtk3_dep, gmodule_dep],
  install: false,
)
