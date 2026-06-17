<!-- TOC -->
  * [Obj](#obj)
    * [ObjString](#objstring)
  * [Value](#value)
    * [Вспомогательные макросы](#вспомогательные-макросы)
  * [Память](#память)
    * [Строки](#строки)
<!-- TOC -->

## Obj
Представление объекта. Все структуры должны включать в себя базовую структуру Obj
```c++
typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    Obj* next;
};
```

### ObjString
Строка. Включает в себя базовую структуру, длину строки, указатель на начало и хеш.
```c++
struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};
```

## Value
Представление значения. Содержит тип (BOOL, NIL, NUMBER, OBJ) и само значение (bool/double/Obj*)
```c++
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;
```
Сравнение двух значений.
```c++
bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:       return true;
        case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:       return AS_OBJ(a) == AS_OBJ(b);
        default:            return false;
    }
}
```
Печать значения в консоль.
```c++
void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true": "false");
            break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: printf("%s", AS_CSTRING(value)); break;
    }
}
```

### Вспомогательные макросы
Проверка типа значения
```c++
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)
```
Получение фактического значения
```c++
#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJ(value)       ((value).as.obj)
```
Создание объекта значения
```c++
#define BOOL_VAL(value)     ((Value) {VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value) {VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value) {VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((Value) {VAL_OBJ, {.obj = (Obj*) object}})
```

## Память

Выделение нового участка памяти или увеличение существующего.
```c++
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1); // кончилась память
    return result;
}
```

Базовая функция создания объекта. Принимает `size` фактический размер для выделения участка памяти. `ObjType` логический
тип объекта (OBJ_STRING и тд)
```c++
#define ALLOCATE_OBJ(type, objectType) \
    (type*) allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*) reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;
    return object;
}
```

Очистка памяти
```c++
static void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*) object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
        }
    }
}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}
```

### Строки
Функция создания строки. Вызывает базовую ф-ию создания объекта и заполняет собственные поля. Указатель на созданный
объект строки складывается в хеш таблицу в качестве ключа и значением NIL_VAL.
```c++
static ObjString* allocateString(char *chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NIL_VAL);
    return string;
}
```

Поиск строки в хеш таблице. Все строки в языке интернированные, то есть их можно будет сравнивать знаком ==, поскольку 
при последующих получениях одной и той же строки будет возвращаться один и тот же указатель.
```c++
ObjString *tableFindString(Table *table, const char *chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash &&
            memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}
```

Хеш функция
```c++
static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}
```

Функция получения или создания строки. В случае если строка была найдена, то chars очищается из памяти.
```c++

ObjString *takeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}
```

Функция создания или получения строки. В случае если строка не была найдена, то делается копия chars
```c++
ObjString *copyString(const char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL) return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}
```