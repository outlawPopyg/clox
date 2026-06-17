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

Группировка. Парсит вложенное выражение с приоритетом PREC_ASSIGMENT (1). Остановится когда встретит знак с нулевым приоритетом
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

Считваются два токена. Для первого выполняется prefixRule, далее если приоритет контекста (precedence) не больше приоритета
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
16. Вызываем binary(SUB + 1). Сдвигаемся `1 EOF`. Кладем константу `OP_CONST 1, OP_CONST 2, OP_CONST 3, OP_MULTIPLY, OP_ADD, OP_CONST 1`.
17. Проверяем SUB > EOF --> выходим из функции возвращаемся в binary() и кладем минус

итого `OP_CONST 1, OP_CONST 2, OP_CONST 3, OP_MULTIPLY, OP_ADD, OP_CONST 1, OP_SUB`

`2 * (1 + 2)`

1. Считывается `2 *`. Константа кладется в чанк `OP_CONST 2` и вызывается обработка для умножения. 
2. Считывается `( 1`. Вызывается grouping(), который вызывает parsePrecedence(PREC_ASSIGMENT)
3. Считывается `1 +`. Константа кладется в чанк `OP_CONST 2, OP_CONST 1`.
4. Проверяется PREC_ASSIGMENT <= PREC_ADD --> вызываем обработку для сложения
5. Считываем `2 )`. Кладем константу в чанк `OP_CONST 2, OP_CONST 1, OP_CONST 2`
6. У закрывающейся скобки нулевой приоритет поэтому выходим из секции плюса и кладем его в чанк `OP_CONST 2, OP_CONST 1, OP_CONST 2, OP_ADD`
7. Возвращаемся в parsePrecedence(PREC_ASSIGMENT) проверяем PREC_ASSIGMENT > PREC_NONE выходим
8. Попадаем в grouping и проверяем что текущий токен это закрывающаяся скобка и сдвигаемся `) EOF`
9. Выходим из grouping и попадаем в parsePrecedence(MULTIPLY + 1). Проверяем PREC_MULTIPLY > PREC_NONE. Выходим и ставим умножение

итого `OP_CONST 2, OP_CONST 1, OP_CONST 2, OP_ADD, OP_MULTIPLY`