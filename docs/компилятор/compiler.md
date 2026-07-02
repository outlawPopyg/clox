## Компиляция

Объект парсера токенов. Держит два идущих подряд токена

```c++
typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;
```

Структура с правилами парсинга. prefix - функция для обработки префиксного токена, infix - инфиксная.
precedence - приоритет операций. Чем меньше приоритет, тем позднее байт код этой операции попадет в чанк

```c++
typedef void (*ParseFn)(bool canAssign); // ссылка на функцию

typedef enum {
    PREC_NONE,
    PREC_ASSIGMENT,     // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! -
    PREC_CALL,          // . ()
    PREC_PRIMARY
} Precedence;

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;
```

Непосредственные правила парсинга для каждого токена

```c++
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},         //   (
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},         //   )
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},         //   {
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},         //   }
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},         //   ,
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},         //   .
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},         //   -
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},         //   +
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},         //   ;
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},       //   /
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},       //   *
    [TOKEN_BANG]          = {unary,     NULL,   PREC_NONE},        //   !
    [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},   //   !=
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},         //   =
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},   //   ==
    [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON}, //   >
    [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON}, //   >=
    [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON}, //   <
    [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON}, //   <=
    [TOKEN_IDENTIFIER]    = {variable,     NULL,   PREC_NONE},     //   идентификатор
    [TOKEN_STRING]        = {string,     NULL,   PREC_NONE},       //   "
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},         //   число
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},         //   &
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},         //   class
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},         //   else
    [TOKEN_FALSE]         = {literal,     NULL,   PREC_NONE},      //   false
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},         //   for
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},         //   fun
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},         //   if
    [TOKEN_NIL]           = {literal,     NULL,   PREC_NONE},      //   nil
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},         //   or
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},         //   print
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},         //   return
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},         //   super
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},         //   this
    [TOKEN_TRUE]          = {literal,     NULL,   PREC_NONE},      //   true
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},         //   var
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},         //   while
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},         //   error
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},         //   eof
};
```

### Префиксные обработчики

Строки, числа и литералы. Просто извлекаются токены и кладутся в чанк.

```c++
static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return;
    }
}
```

Унарный минус и отрицание. Вызывают парсинг вложенного выражения с приоритетом PREC_UNARY (8).
Довольно высокий приоритет, а значит он попадет в чанк быстрее остальных.

```c++
static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return;
    }
}
```

Группировка. Парсит вложенное выражение с приоритетом PREC_ASSIGMENT (1). Остановится когда встретит знак с нулевым
приоритетом
и проверит что этот знак должен быть закрывающейся скобкой.

```c++
static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void expression() {
    parsePrecedence(PREC_ASSIGMENT);
}
```

### Инфиксные обработчики

Бинарные операции (+,-,== и т.д). Вызывают парсинг вложенного выражения с precedence + 1, посольку операции в языке
левоассоциотивные.

```c++
static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
        case TOKEN_GREATER: emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_LESS: emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_PLUS: emitByte(OP_ADD); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
        default: return;
    }
}
```

### Основаная функция парсинга

Считваются два токена. Для первого выполняется prefixRule, далее если приоритет контекста (precedence) не больше
приоритета
следующего токена, то выполняется обработка последующих операций.

```c++
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGMENT;
    prefixRule(canAssign);
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assigment target.");
    }
}
```

### Примеры

`1 + 2 * 3 - 1`

1. Читаем первые два токена `1 +`
2. Обрабатываем префикс: вызываем number() в чанк кладется OP_CONST 1
3. Текущий precedence = 1, он <= приоритета знака сложения, значит считываем токены дальше `+ 2`
4. Обрабатываем инфиксную часть: вызываем binary()
5. binary() вызывает parsePrecedence(ADD + 1). Внутри снова сдвигаемся `2 *`
6. Кладем константу в чанк: `OP_CONST 1, OP_CONST 2`
7. Проверяем что ADD + 1 <= MULTIPLY, значит разбираем выражение дальше. Сдвигаемся `* 3`
8. Обрабатываем инфиксную часть: вызываем binary()
9. binary() вызываем parsePrecedence(MULTIPLY + 1). Внутри сдвигаемся `3 -`.
10. Кладем константу в чанк: `OP_CONST 1, OP_CONST 2, OP_CONST 3`.
11. Проверяем MULTIPLY + 1 > SUB. Следовательно в цикл не заходим и выходим из parsePrecedence(MULTIPLY + 1)
12. Попадаем в binary() где кладем в чанк умножение `OP_MULTIPLY, OP_CONST 3, OP_CONST 2, OP_CONST 1`.
13. Возвращаемся в parsePrecedence(ADD + 1). В цикле проверяем что ADD + 1 > SUB. Следовательно выходим из функции.
14. Попадаем в binary() где кладем в чанк сложение `OP_CONST 1, OP_CONST 2, OP_CONST 3, OP_MULTIPLY, OP_ADD`
15. Возвращаемся в parsePrecedence(ASSIGMENT) проверяем что ASSIGMENT <= SUB. Заходим внутрь с сдвигаеся `- 1`
16. Вызываем binary(SUB + 1). Сдвигаемся `1 EOF`. Кладем константу
    `OP_CONST 1, OP_CONST 2, OP_CONST 3, OP_MULTIPLY, OP_ADD, OP_CONST 1`.
17. Проверяем SUB > EOF --> выходим из функции возвращаемся в binary() и кладем минус

итого `OP_CONST 1, OP_CONST 2, OP_CONST 3, OP_MULTIPLY, OP_ADD, OP_CONST 1, OP_SUB`

`2 * (1 + 2)`

1. Считывается `2 *`. Константа кладется в чанк `OP_CONST 2` и вызывается обработка для умножения.
2. Считывается `( 1`. Вызывается grouping(), который вызывает parsePrecedence(PREC_ASSIGMENT)
3. Считывается `1 +`. Константа кладется в чанк `OP_CONST 2, OP_CONST 1`.
4. Проверяется PREC_ASSIGMENT <= PREC_ADD --> вызываем обработку для сложения
5. Считываем `2 )`. Кладем константу в чанк `OP_CONST 2, OP_CONST 1, OP_CONST 2`
6. У закрывающейся скобки нулевой приоритет поэтому выходим из секции плюса и кладем его в чанк
   `OP_CONST 2, OP_CONST 1, OP_CONST 2, OP_ADD`
7. Возвращаемся в parsePrecedence(PREC_ASSIGMENT) проверяем PREC_ASSIGMENT > PREC_NONE выходим
8. Попадаем в grouping и проверяем что текущий токен это закрывающаяся скобка и сдвигаемся `) EOF`
9. Выходим из grouping и попадаем в parsePrecedence(MULTIPLY + 1). Проверяем PREC_MULTIPLY > PREC_NONE. Выходим и ставим
   умножение

итого `OP_CONST 2, OP_CONST 1, OP_CONST 2, OP_ADD, OP_MULTIPLY`

### Обработка ошибок

Ошибка пишется один раз за выражение.

```c++
static void errorAtCurrent(const char *message) {
    errorAt(&parser.current, message);
}

static void error(const char *message) {
    errorAt(&parser.previous, message);
}

static void errorAt(Token *token, const char *message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}
```

После токены проматываются до safe point'ов и флаг ошибки сбрасывается.

```c++
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default: ;
        }

        advance();
    }
}
```

Пример

`(1 ++ 2) - 4;`

Путь этого выражения будет таким:

```c++
while (!match(TOKEN_EOF)) {
    declaration();
}

static void declaration() {
    if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}
```

В parsePrecedence сперва мы заходим в grouping

```c++
static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}
```

Потом обрабатываем знак сложения и когда попытаемся вызвать prefix() для второго знака сложения получим первую ошибку

```c++
if (prefixRule == NULL) {
    error("Expect expression");
    return;
}
```

Далее мы выходим из функции для сложения обратно в функцию обработки группировки тут пишем еще одну ошибку "Expect ')' after expression".
Теперь возвращаемся в первую функцию из нее попадаем в expressionStatement где пишем третью ошибку "Expect ';' after expression."

Теперь доходим до этой строчки 

`if (parser.panicMode) synchronize();`

которая пролистает токены до ';' и снимет флаг `panicMode`. Если бы мы не делали обработку лавинных ошибок то у нас вместо одной ошибки
где мы случайно добавили лишний плюс было бы три которые бы сбили с толку. После сброса компиляция продолжится 
для выявления еще потенциальных ошибок, но основной флаг `hadError` не сбросится поскольку при одной ошибке программа уже не рабочая. 

### Локальные переменные
```c++
typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;
```
Локальные переменные объявляются внутри блока. Блоки ниже уровнем видят локальные переменные выше и могут переопределять
локальные переменные с тем же именем выше
```c++
static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}
```

После выхода из блока в чанк добавляется операции изъятия элемента из стека равное количеству локальных переменных в блоке.

#### Объявление локальной переменной

При объявлении начинается обход с конца массива локальных переменных, то есть тех, у которых самая низкий уровень и проверяется
нет ли на текущем уровне переменных с таким же именем. Если встречается переменная уровнем выше текущего то поиск дальше не имеет смысла.
Создается новая переменная с промежуточным уровнем -1 и добавляется в статический массив

```c++
static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    defineVariable(global);
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void declareVariable() {
    if (current->scopeDepth == 0) return; // если глобальная

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            // если встречаем локальную переменную уровнем выше то дальше проверять смысла нет
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this same name in this scope.");
        }
    }
    addLocal(*name);
}

static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}
```

Дальше проверяется есть ли знак присваивания, если да, то парсится выражение иначе кладется nil.
После чего текущей локальной переменной присваивается уровень вложенности.

```c++
static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void markInitialized() {
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}
```

То есть на данном этапе в стек мы положили результат вычисления выражения и его оттуда не убрали как мы обычно делаем при обычном statement.
Также сама локальная перменная лежит во временном массиве для компиляции.
То есть при объявлении мы не кладем в чанк что то вроде DEFINE_LOCAL, а просто у нас индекс во временном массиве совпадает с индексом рантайм стека.

#### Получение и установка локальной переменной

`a + 2;`
Сразу попадаем в `namedVariable` с `canAssign = true` (про него позже, когда локальная переменная просто читается этот параметр ни на что не влияет)
Переменная ищется с конца, таким образом если уровнем выше будет переменная с таким же именем до нее просто не дойдет поиск и вернется первая найденная.
`resolveLocal` возвращает индекс где в данный момент должна лежать локальная переменная в рантайм стеке. В чанк кладется `OP_GET_LOCAL` и этот индекс.

`a = 1 + 2;`
Тоже попадаем в namedVariable с canAssign = true дальше парсим выражение после знака присваивания и кладем в чанк OP_SET_LOCAL и индекс.
В рантайм стеке значение по индексу изменится.

Выражения с ошибками:

`1 = 2;`
* canAssign будет true
* для 1 вызовется prefixRule() 
* precedence 'PREC_ASSIGMENT' > `=`. В цикл не заходим
* получаем ошибку Invalid assigment target

`a + 1 * 2 = 3`
* canAssign = true
  * `+`: canAssign = false
    * `*`: canAssign = false
    * `*`: precedence > '=' --> выходим
  * `+`: precedence > '=' --> выходим
* precedence > '='
* `canAssign && match(TOKEN_EQUAL)` == true --> error("Invalid assigment target")

То есть смысл canAssign в том, может ли переменная взять в себя выражение после знака присваивания

```c++
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGMENT;
    prefixRule(canAssign);
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assigment target.");
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t) arg);
    } else {
        emitBytes(getOp, (uint8_t) arg);
    }
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer");
            }
            return i;
        }
    }

    return -1;
}
```

В рантайм стеке локальная переменная просто кладется еще раз наверх стека, и по прежнему остается внизу пока 
ее не уберет POP после выхода из блока

```c++
case OP_GET_LOCAL: {
    uint8_t slot = READ_BYTE();
    push(vm.stack[slot]);
    break;
}
case OP_SET_LOCAL: {
    uint8_t slot = READ_BYTE();
    vm.stack[slot] = peek(0);
    break;
}
```