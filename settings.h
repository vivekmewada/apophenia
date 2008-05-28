#ifndef __apop_settings_h__
#define __apop_settings_h__

#include "types.h"
#include "asst.h"

#undef __BEGIN_DECLS    /* extern "C" stuff cut 'n' pasted from the GSL. */
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS /* empty */
# define __END_DECLS /* empty */
#endif

__BEGIN_DECLS

void * apop_settings_get_group(apop_model *m, char *type);
void apop_settings_rm_group(apop_model *m, char *delme);
void apop_settings_copy_group(apop_model *outm, apop_model *inm, char *copyme);

__END_DECLS

#define Apop_settings_get_group(m, type) apop_settings_get_group(m, #type)
#define Apop_settings_rm_group(m, type) apop_settings_rm_group(m, #type)

#define Apop_settings_model_copy(newm, m, type, ...) apop_model *newm = apop_model_copy(m); Apop_settings_add_group(newm, type, __VA_ARGS__);

#define Apop_settings_add_group(model, type, ...)  do {     \
        apop_assert_void(!apop_settings_get_group(model, #type), 0, 's', "You're trying to add a setting group of type " #type " to " #model " but that model already has such a group."); \
        (model)->settings = realloc((model)->settings, sizeof(apop_settings_type)*((model)->setting_ct+1));   \
        strncpy((model)->settings[(model)->setting_ct].name , #type , 100) ;    \
        (model)->settings[(model)->setting_ct].setting_group = type ##_settings_alloc (__VA_ARGS__);    \
        (model)->settings[(model)->setting_ct].free = type ## _settings_free; \
        (model)->settings[(model)->setting_ct].copy = type ## _settings_copy; \
        ((model)->setting_ct) ++;   \
    } while (0);

#define Apop_settings_alloc_add(model, type, setting, data, ...)  \
    do {                                                \
        Apop_settings_add_group(model, type, __VA_ARGS__)           \
        Apop_settings_add(model, type, setting, data)       \
    } while (0);


#define Apop_settings_get(model, type, setting)  \
    (((type ## _settings *) apop_settings_get_group(model, #type))->setting)

#define Apop_settings_add(model, type, setting, data)  \
    apop_assert_void(apop_settings_get_group(model, #type), 0, 's', "You're trying to modify a setting in " #model "'s setting group of type " #type " but that model doesn't have such a group."); \
    ((type ## _settings *) apop_settings_get_group(model, #type))->setting = (data);

#define APOP_SETTINGS_ADD Apop_settings_add
#define APOP_SETTINGS_ALLOC_ADD Apop_settings_alloc_add
#define APOP_SETTINGS_GET Apop_settings_get
#define APOP_SETTINGS_ADD_GROUP Apop_settings_add_group
#define APOP_SETTINGS_GET_GROUP Apop_settings_get_group
#define APOP_SETTINGS_RM_GROUP Apop_settings_rm_group

#endif
