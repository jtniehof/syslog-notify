/*Dump out the capability list of a notification server
 *Compile with:
 *gcc `pkg-config --cflags --libs libnotify` getcaps.c
 */

#include <libnotify/notify.h>
#include <stdio.h>

void PrintString(gconstpointer str,
		 gconstpointer junk) {
  printf("%s\n", (char*)str);
}

int main() {
  GList *caps;
  notify_init("cap dumper");
  caps=notify_get_server_caps();
  if(caps) {
    g_list_foreach(caps, (GFunc)PrintString, NULL);
    g_list_foreach(caps, (GFunc)g_free, NULL);
    g_list_free(caps);
  }
  notify_uninit();
}
