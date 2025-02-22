#include <tree_sitter/api.h>
#include <stdio.h>
#include <string.h>

// Declare the Python parser (since that's what we're linking against)
const TSLanguage *tree_sitter_python(void);

int main() {
    // Create a parser
    TSParser *parser = ts_parser_new();
    
    // Set the parser's language (Python in this case)
    ts_parser_set_language(parser, tree_sitter_python());
    
    // Example source code to parse
    const char *source_code = "def main(): return 0";
    
    // Parse the source code
    TSTree *tree = ts_parser_parse_string(
        parser,
        NULL,  // changed from nullptr
        source_code,
        strlen(source_code)
    );
    
    // Get the root node
    TSNode root_node = ts_tree_root_node(tree);
    
    // Print the syntax tree as an S-expression
    char *tree_string = ts_node_string(root_node);
    printf("Syntax tree: %s\n", tree_string);
    
    // Cleanup
    free(tree_string);
    ts_tree_delete(tree);
    ts_parser_delete(parser);
    
    return 0;
}