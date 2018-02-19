#include "PBRTParser.cpp"

int main(int argc, char** argv){
    if (argc < 2){
        printf("Usage: parse \"string\"\n");
        exit(1);
    }
    auto istr = std::string(argv[1]);

    auto lex = PBRTLexer(istr);
    Lexeme lexm;
    lex.get_next_lexeme(lexm);

    std::cout << lexm.get_value() << std::endl;
}