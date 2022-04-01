#include <stdio.h>
#include <glib.h>

int main()
{
    GList* list = NULL;
    list = g_list_append(list, "Hello world!");
    printf("The first item in the list is '%s'\n", g_list_first(list)->data);
    return 0;
}