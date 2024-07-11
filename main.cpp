#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Split 256

typedef struct State {
    int c;
    struct State* out;
    struct State* out1;
    int lastlist;
} State;

typedef struct Ptrlist {
    State** s;
    struct Ptrlist* next;
} Ptrlist;

typedef struct Frag {
    State* start;
    Ptrlist* out;
} Frag;

typedef struct List {
    State** s;
    int n;
} List;

State* matchstate = &(State){0};
int listid = 0;

State* state(int c, State* out, State* out1);
Frag frag(State* start, Ptrlist* out);
Ptrlist* list1(State** outp);
Ptrlist* append(Ptrlist* l1, Ptrlist* l2);
void patch(Ptrlist* l, State* s);
void addstate(List* l, State* s);
List* startlist(State* s, List* l);
void step(List* clist, int c, List* nlist);
int ismatch(List* l);
int match(State* start, char* s);
State* post2nfa(char* postfix);
char* re2post(char* re);
void print_nfa(State* start);

int main() {
    char* regex = "a?na";
    char* postfix = re2post(regex);
    State* start = post2nfa(postfix);
    char* string = "an";
    
    if (match(start, string)) {
        printf("Matched\n");
    } else {
        printf("Not matched\n");
    }
    
    free(postfix);
    return 0;
}

State* state(int c, State* out, State* out1) {
    State* s = (State*)malloc(sizeof(State));
    s->c = c;
    s->out = out;
    s->out1 = out1;
    s->lastlist = 0;
    return s;
}

Frag frag(State* start, Ptrlist* out) {
    Frag n = {start, out};
    return n;
}

Ptrlist* list1(State** outp) {
    Ptrlist* l = (Ptrlist*)malloc(sizeof(Ptrlist));
    l->s = outp;
    l->next = NULL;
    return l;
}

Ptrlist* append(Ptrlist* l1, Ptrlist* l2) {
    Ptrlist* oldl1;

    oldl1 = l1;
    while (l1->next)
        l1 = l1->next;
    l1->next = l2;
    return oldl1;
}

void patch(Ptrlist* l, State* s) {
    Ptrlist* next;

    for (; l; l = next) {
        next = l->next;
        *l->s = s;
    }
}

void addstate(List* l, State* s) {
    if (s == NULL || s->lastlist == listid)
        return;
    s->lastlist = listid;
    if (s->c == Split) {
        addstate(l, s->out);
        addstate(l, s->out1);
        return;
    }
    l->s[l->n++] = s;
}

List* startlist(State* s, List* l) {
    l->n = 0;
    listid++;
    addstate(l, s);
    return l;
}

void step(List* clist, int c, List* nlist) {
    int i;
    State* s;

    listid++;
    nlist->n = 0;
    for (i = 0; i < clist->n; i++) {
        s = clist->s[i];
        if (s->c == c)
            addstate(nlist, s->out);
    }
}

int ismatch(List* l) {
    int i;
    for (i = 0; i < l->n; i++) {
        if (l->s[i] == matchstate)
            return 1;
    }
    return 0;
}

int match(State* start, char* s) {
    List l1, l2;
    List *clist, *nlist, *t;

    clist = startlist(start, &l1);
    nlist = &l2;
    for (; *s; s++) {
        step(clist, *s & 0xFF, nlist);
        t = clist;
        clist = nlist;
        nlist = t;
    }
    return ismatch(clist);
}

State* post2nfa(char* postfix) {
    char* p;
    Frag stack[1000], *stackp, e1, e2, e;
    State* s;

    #define push(s) *stackp++ = s
    #define pop()   *--stackp

    stackp = stack;
    for (p = postfix; *p; p++) {
        switch (*p) {
        default:
            s = state(*p, NULL, NULL);
            push(frag(s, list1(&s->out)));
            break;
        case '.':
            e2 = pop();
            e1 = pop();
            patch(e1.out, e2.start);
            push(frag(e1.start, e2.out));
            break;
        case '|':
            e2 = pop();
            e1 = pop();
            s = state(Split, e1.start, e2.start);
            push(frag(s, append(e1.out, e2.out)));
            break;
        case '?':
            e = pop();
            s = state(Split, e.start, NULL);
            push(frag(s, append(e.out, list1(&s->out1))));
            break;
        case '*':
            e = pop();
            s = state(Split, e.start, NULL);
            patch(e.out, s);
            push(frag(s, list1(&s->out1)));
            break;
        case '+':
            e = pop();
            s = state(Split, e.start, NULL);
            patch(e.out, s);
            push(frag(e.start, list1(&s->out1)));
            break;
        }
    }

    e = pop();
    patch(e.out, matchstate);
    return e.start;

    #undef push
    #undef pop
}

char* re2post(char* re) {
    int nalt = 0, natom = 0;
    static char buf[8000];
    char* dst = buf;
    struct {
        int nalt;
        int natom;
    } paren[100], *p;

    p = paren;
    if (strlen(re) >= sizeof buf / 2)
        return NULL;
    for (; *re; re++) {
        switch (*re) {
        case '(':
            if (natom > 1) {
                --natom;
                *dst++ = '.';
            }
            if (p >= paren + 100)
                return NULL;
            p->nalt = nalt;
            p->natom = natom;
            p++;
            nalt = 0;
            natom = 0;
            break;
        case '|':
            if (natom == 0)
                return NULL;
            while (--natom > 0)
                *dst++ = '.';
            nalt++;
            break;
        case ')':
            if (p == paren)
                return NULL;
            if (natom == 0)
                return NULL;
            while (--natom > 0)
                *dst++ = '.';
            for (; nalt > 0; nalt--)
                *dst++ = '|';
            --p;
            nalt = p->nalt;
            natom = p->natom;
            natom++;
            break;
        case '*':
        case '+':
        case '?':
            if (natom == 0)
                return NULL;
            *dst++ = *re;
            break;
        default:
            if (natom > 1) {
                --natom;
                *dst++ = '.';
            }
            *dst++ = *re;
            natom++;
            break;
        }
    }
    if (p != paren)
        return NULL;
    while (--natom > 0)
        *dst++ = '.';
    for (; nalt > 0; nalt--)
        *dst++ = '|';
    *dst = 0;
    return buf;
}
