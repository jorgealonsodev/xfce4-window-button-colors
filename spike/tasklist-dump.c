#include <string.h>
#include <gmodule.h>
#include <gtk/gtk.h>

static gboolean
hook (GSignalInvocationHint *i, guint n, const GValue *pv, gpointer d)
{
  GtkWidget *w = GTK_WIDGET (g_value_get_object (&pv[0]));
  const gchar *nm = gtk_widget_get_name (w);
  const gchar *tn = G_OBJECT_TYPE_NAME (w);
  GtkWidget *par = gtk_widget_get_parent (w);
  const gchar *pn = par ? G_OBJECT_TYPE_NAME (par) : "-";
  (void) i; (void) n; (void) d;
  g_message ("DUMP type=%-24s name=%-20s parent=%s", tn, nm ? nm : "(null)", pn);
  return TRUE;
}

G_MODULE_EXPORT void gtk_module_init (gint *argc, gchar ***argv);
G_MODULE_EXPORT void
gtk_module_init (gint *argc, gchar ***argv)
{
  (void) argc; (void) argv;
  if (g_strcmp0 (g_get_prgname (), "xfce4-panel") != 0)
    return;
  g_signal_add_emission_hook (g_signal_lookup ("map", GTK_TYPE_WIDGET), 0, hook, NULL, NULL);
}
