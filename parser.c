#ifndef MINISHELL_H
#define MINISHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CMD_LEN 1024
#define MAX_TOKENS 128
#define MAX_TOKEN_LEN 256
#define MAX_HEREDOC_SIZE 4096

typedef enum {
    TOKEN_WORD,        // Command or argument
    TOKEN_PIPE,        // |
    TOKEN_REDIRECT_IN, // <
    TOKEN_HEREDOC,     // <<
    TOKEN_REDIRECT_OUT,// >
    TOKEN_APPEND_OUT,  // >>
    TOKEN_BACKGROUND,  // &
    TOKEN_SEMICOLON,   // ;
    TOKEN_EOF          // End of input
} TokenType;

typedef struct s_token
{
    TokenType type;
    char value[MAX_TOKEN_LEN];
} t_token;

typedef struct s_command
{
    char *cmd;
    char *args[MAX_TOKENS];
    int arg_count;
    char *input_file;    // For < redirection
    char *heredoc;       // For << here document
    char *output_file;   // For > and >> redirection
    int append_output;   // Flag for >> (append mode)
    int run_in_background;
} t_command;

typedef struct s_parsed_input {
    t_command commands[MAX_TOKENS];
    int cmd_count;
} t_parsed_input;

// Function prototypes
t_token *tokenize(const char *input, int *token_count);
t_parsed_input parse_input(t_token *tokens, int token_count);
void free_parsed_input(t_parsed_input *parsed);
void print_tokens(t_token *tokens, int token_count);
void print_parsed_input(t_parsed_input *parsed);
int is_special_char(char c);
char *read_heredoc(const char *delimiter);

#endif // MINISHELL_H

int is_special_char(char c) {
    return c == '|' || c == '<' || c == '>' || c == '&' || c == ';';
}

// Read a here document from standard input
char *read_heredoc(const char *delimiter) {
    char *buffer = malloc(MAX_HEREDOC_SIZE);
    if (!buffer) {
        perror("Failed to allocate memory for here document");
        exit(EXIT_FAILURE);
    }
    
    buffer[0] = '\0'; // Initialize empty string
    char line[MAX_CMD_LEN];
    int total_len = 0;
    
    printf("heredoc> ");
    fflush(stdout);
    
    while (fgets(line, sizeof(line), stdin)) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';
        
        // Check if this line matches the delimiter
        if (strcmp(line, delimiter) == 0) {
            break;
        }
        
        // Append the line to our buffer (add back the newline)
        int line_len = strlen(line);
        if (total_len + line_len + 2 < MAX_HEREDOC_SIZE) { // +2 for newline and null terminator
            strcat(buffer, line);
            strcat(buffer, "\n");
            total_len += line_len + 1;
        } else {
            fprintf(stderr, "Warning: Here document exceeds maximum size\n");
            break;
        }
        
        printf("heredoc> ");
        fflush(stdout);
    }
    
    return buffer;
}

// Tokenize the input string
t_token *tokenize(const char *input, int *token_count) {
    t_token *tokens = malloc(sizeof(t_token) * MAX_TOKENS);
    if (!tokens) {
        perror("Failed to allocate memory for tokens");
        exit(EXIT_FAILURE);
    }

    *token_count = 0;
    int input_len = strlen(input);
    int i = 0;

    while (i < input_len && *token_count < MAX_TOKENS - 1) {
        // Skip whitespace
        while (i < input_len && isspace(input[i])) {
            i++;
        }

        if (i >= input_len) {
            break;
        }

        // Handle special characters
        if (is_special_char(input[i])) {
            if (input[i] == '|') {
                tokens[*token_count].type = TOKEN_PIPE;
                strcpy(tokens[*token_count].value, "|");
                (*token_count)++;
                i++;
            } else if (input[i] == '<') {
                if (i + 1 < input_len && input[i + 1] == '<') {
                    tokens[*token_count].type = TOKEN_HEREDOC;
                    strcpy(tokens[*token_count].value, "<<");
                    (*token_count)++;
                    i += 2;
                } else {
                    tokens[*token_count].type = TOKEN_REDIRECT_IN;
                    strcpy(tokens[*token_count].value, "<");
                    (*token_count)++;
                    i++;
                }
            } else if (input[i] == '>') {
                if (i + 1 < input_len && input[i + 1] == '>') {
                    tokens[*token_count].type = TOKEN_APPEND_OUT;
                    strcpy(tokens[*token_count].value, ">>");
                    (*token_count)++;
                    i += 2;
                } else {
                    tokens[*token_count].type = TOKEN_REDIRECT_OUT;
                    strcpy(tokens[*token_count].value, ">");
                    (*token_count)++;
                    i++;
                }
            } else if (input[i] == '&') {
                tokens[*token_count].type = TOKEN_BACKGROUND;
                strcpy(tokens[*token_count].value, "&");
                (*token_count)++;
                i++;
            } else if (input[i] == ';') {
                tokens[*token_count].type = TOKEN_SEMICOLON;
                strcpy(tokens[*token_count].value, ";");
                (*token_count)++;
                i++;
            }
        } else {
            // Handle words (commands and arguments)
            tokens[*token_count].type = TOKEN_WORD;
            int j = 0;

            // Handle quoted strings
            if (input[i] == '"' || input[i] == '\'') {
                char quote = input[i];
                i++; // Skip the opening quote
                
                while (i < input_len && input[i] != quote && j < MAX_TOKEN_LEN - 1) {
                    tokens[*token_count].value[j++] = input[i++];
                }
                
                if (i < input_len) {
                    i++; // Skip the closing quote
                }
            } else {
                // Regular word
                while (i < input_len && !isspace(input[i]) && !is_special_char(input[i]) && j < MAX_TOKEN_LEN - 1) {
                    tokens[*token_count].value[j++] = input[i++];
                }
            }
            
            tokens[*token_count].value[j] = '\0';
            (*token_count)++;
        }
    }

    // Add end-of-input token
    tokens[*token_count].type = TOKEN_EOF;
    strcpy(tokens[*token_count].value, "EOF");
    (*token_count)++;

    return tokens;
}

// Parse the tokens into a command structure
t_parsed_input parse_input(t_token *tokens, int token_count) {
    t_parsed_input parsed = {0};
    t_command *current_cmd = &parsed.commands[0];
    parsed.cmd_count = 1;

    int i = 0;
    while (i < token_count && tokens[i].type != TOKEN_EOF) {
        switch (tokens[i].type) {
            case TOKEN_WORD:
                if (current_cmd->cmd == NULL) {
                    // This is the command name
                    current_cmd->cmd = strdup(tokens[i].value);
                    current_cmd->args[current_cmd->arg_count++] = strdup(tokens[i].value);
                } else {
                    // This is an argument
                    current_cmd->args[current_cmd->arg_count++] = strdup(tokens[i].value);
                }
                i++;
                break;

            case TOKEN_REDIRECT_IN:
                i++; // Move to the file name
                if (i < token_count && tokens[i].type == TOKEN_WORD) {
                    current_cmd->input_file = strdup(tokens[i].value);
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after <\n");
                    i++;
                }
                break;

            case TOKEN_HEREDOC:
                i++; // Move to the delimiter
                if (i < token_count && tokens[i].type == TOKEN_WORD) {
                    // Read the here document
                    current_cmd->heredoc = read_heredoc(tokens[i].value);
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected delimiter after <<\n");
                    i++;
                }
                break;

            case TOKEN_REDIRECT_OUT:
                i++; // Move to the file name
                if (i < token_count && tokens[i].type == TOKEN_WORD) {
                    current_cmd->output_file = strdup(tokens[i].value);
                    current_cmd->append_output = 0;
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after >\n");
                    i++;
                }
                break;

            case TOKEN_APPEND_OUT:
                i++; // Move to the file name
                if (i < token_count && tokens[i].type == TOKEN_WORD) {
                    current_cmd->output_file = strdup(tokens[i].value);
                    current_cmd->append_output = 1;
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after >>\n");
                    i++;
                }
                break;

            case TOKEN_BACKGROUND:
                current_cmd->run_in_background = 1;
                i++;
                break;

            case TOKEN_PIPE:
                // Finish the current command and start a new one
                if (parsed.cmd_count < MAX_TOKENS) {
                    current_cmd = &parsed.commands[parsed.cmd_count++];
                    i++;
                } else {
                    fprintf(stderr, "Error: Too many commands in pipeline\n");
                    i++;
                }
                break;

            case TOKEN_SEMICOLON:
                // Finish the current command sequence and start a new one
                if (parsed.cmd_count < MAX_TOKENS) {
                    current_cmd = &parsed.commands[parsed.cmd_count++];
                    i++;
                } else {
                    fprintf(stderr, "Error: Too many commands\n");
                    i++;
                }
                break;

            default:
                // Should not reach here
                i++;
                break;
        }
    }

    // Ensure each command's argument list is NULL-terminated
    for (int j = 0; j < parsed.cmd_count; j++) {
        parsed.commands[j].args[parsed.commands[j].arg_count] = NULL;
    }

    return parsed;
}

// Helper function to free allocated memory
void free_parsed_input(t_parsed_input *parsed) {
    for (int i = 0; i < parsed->cmd_count; i++) {
        if (parsed->commands[i].cmd)
            free(parsed->commands[i].cmd);
        
        for (int j = 0; j < parsed->commands[i].arg_count; j++) {
            if (parsed->commands[i].args[j])
                free(parsed->commands[i].args[j]);
        }
        
        if (parsed->commands[i].input_file)
            free(parsed->commands[i].input_file);
        if (parsed->commands[i].heredoc)
            free(parsed->commands[i].heredoc);
        if (parsed->commands[i].output_file)
            free(parsed->commands[i].output_file);
    }
}

// Print tokens for debugging
void print_tokens(t_token *tokens, int token_count) {
    printf("Tokens:\n");
    for (int i = 0; i < token_count; i++) {
        printf("  Token %d: type=%d, value='%s'\n", 
               i, tokens[i].type, tokens[i].value);
    }
    printf("\n");
}

// Print parsed input for debugging
void print_parsed_input(t_parsed_input *parsed) {
    printf("Parsed Commands (%d):\n", parsed->cmd_count);
    
    for (int i = 0; i < parsed->cmd_count; i++) {
        t_command *cmd = &parsed->commands[i];
        
        printf("  Command %d: %s\n", i + 1, cmd->cmd);
        
        printf("    Arguments (%d): ", cmd->arg_count);
        for (int j = 0; j < cmd->arg_count; j++) {
            printf("'%s' ", cmd->args[j]);
        }
        printf("\n");
        
        if (cmd->input_file) {
            printf("    Input: %s\n", cmd->input_file);
        }
        
        if (cmd->heredoc) {
            printf("    Here Document: \n----------\n%s----------\n", cmd->heredoc);
        }
        
        if (cmd->output_file) {
            printf("    Output: %s (append: %s)\n", 
                   cmd->output_file, 
                   cmd->append_output ? "yes" : "no");
        }
        
        printf("    Background: %s\n", 
               cmd->run_in_background ? "yes" : "no");
        
        printf("\n");
    }
}

// Example main function showing usage
int main() {
    char input[MAX_CMD_LEN];
    
    while (1) {
        printf("minishell> ");
        fflush(stdout);
        
        if (!fgets(input, MAX_CMD_LEN, stdin)) {
            break;
        }
        
        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';
        
        if (strcmp(input, "exit") == 0) {
            break;
        }
        
        int token_count = 0;
        t_token *tokens = tokenize(input, &token_count);
        print_tokens(tokens, token_count);
        
        t_parsed_input parsed = parse_input(tokens, token_count);
        print_parsed_input(&parsed);
        
        // Here you would execute the parsed commands
        // execute_commands(&parsed);
        
        free_parsed_input(&parsed);
        free(tokens);
    }
    
    printf("Goodbye!\n");
    return 0;
}