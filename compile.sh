clang                                   \
  -I ~/src/tree-sitter/lib/include            \
  ast.c                    \
  ~/src/tree-sitter-python/src/parser.c         \
  ~/src/tree-sitter-python/src/scanner.c         \
  ~/src/tree-sitter/libtree-sitter.a          \
  -o ast