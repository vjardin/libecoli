# Architecture

Most of *libecoli* is written in C. It provides a C API to ease the
creation of interactive command line interfaces. It is split in several
parts:

- the core: it provides the main API to parse and complete strings.
- ecoli nodes: this is the modular part of *libecoli*. Each node
  implements a specific parsing (integers, strings, regular expressions,
  sequence, ...)
- yaml parser: load an ecoli tree described in yaml.
- editline: helpers to instantiate an *editline* and connect it to
  *libecoli*.
- utilities: logging, string, strings vector, hash table, ...

## The grammar graph

The *ecoli nodes* are organized in a graph. Actually, it is mostly a
tree, but loops are allowed in some cases. An *ecoli grammar tree*
describes how the input is parsed and completed. Let's take a simple
example:

![simple-tree.svg](https://raw.githubusercontent.com/rjarry/libecoli/master/doc/simple-tree.svg)

We can also represent it as text like this:

```
sh_lex(
  seq(
    str(foo),
    option(
      str(bar)
    )
  )
)
```

This tree matches the following strings:

- `foo`
- `foo bar`

But, for instance, it does **not** match the following strings:

- `bar`
- `foobar`
- (empty string)

## Parsing an input

When the *libecoli* parses an input, it browses the tree (depth first
search) and returns an *ecoli parse tree*. Let's decompose what is done
when `ec_parse_strvec(root_node, input)` is called, step by step:

1. The initial input is a string vector `["foo bar"]`.
2. The `sh_lex` node splits the input as a shell would have done it, in
   `["foo", "bar"]`, and passes it to its child.
3. The *seq* node (sequence) passes the input to its first child.
4. The *str* node matches if the first element of the string vector is
   `foo`. This is the case, so it returns the number of eaten
   strings (1) to its parent.
5. The *seq* node passes the remaining part `["bar"]` to its next
   child.
6. The option node passes the string vector to its child, which
   matches. It returns 1 to its parent.
7. The *seq* node has no more child, it returns the number of eaten
   strings in the vector (2).
8. Finally, `sh_lex` compares the returned value to the number of
   strings in its initial vector, and return 1 (match) to its caller.

If a node does not match, it returns `EC_PARSE_NOMATCH` to its parent,
and it is propagated until it reaches a node that handle the case. For
instance, the *or* node is able to handle this error. If a child does
not match, the next one is tried until it finds one that matches.

## The parse tree

Let's take another simple example.

![simple-tree2.svg](https://raw.githubusercontent.com/rjarry/libecoli/master/doc/simple-tree2.svg)

In that case, there is no lexer (the `sh_lex` node), so the input must
be a string vector that is already split. For instance, it matches:

- `["foo"]`
- `["bar", "1"]`
- `["bar", "100"]`

But it does **not** match:

- `["bar 1"]`, because there is no `sh_lex` node on the top of the tree
- `["bar"]`
- `[]` (empty vector)

At the time the input is parsed, a *parse tree* is built. When it
matches, it describes which part of the *ecoli grammar graph* that
actually matched, and what input matched for each node.

Passing `["bar", "1"]` to the previous tree would result in the
following *parse tree*:

![parse-tree.svg](https://raw.githubusercontent.com/rjarry/libecoli/master/doc/parse-tree.svg)

Each node of the *parse tree* references the node of the *grammar graph*
that matched. It also stores the string vector that matched.

![parse-tree2.svg](https://raw.githubusercontent.com/rjarry/libecoli/master/doc/parse-tree2.svg)

We can see that not all of the grammar nodes are referenced, since the
str("foo") node was not parsed. Similarly, one grammar node can be
referenced several times in the parse tree, as we can see it in the
following example that matches `[]`, `[foo]`, `[foo, foo]`, ...

![simple-tree3.svg](https://raw.githubusercontent.com/rjarry/libecoli/master/doc/simple-tree3.svg)

Here is the resulting *parse tree* when the input vector is `[foo, foo,
foo]`:

![parse-tree3.svg](https://raw.githubusercontent.com/rjarry/libecoli/master/doc/parse-tree3.svg)

## TODO

- Node identifiers
- completions
- C example
- ec_config
- parse yaml
- params are consumed
- nodes
- attributes
- extending lib with external nodes (in dev guide?)
