/***************************************************************************
 *   Copyright (C) 2002 by Roberto Raggi                                   *
 *   raggi@cli.di.unipi.it                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "parser.h"
#include "driver.h"
#include "lexer.h"
#include "errors.h"
#include "problemreporter.h"

#include <qstring.h>
#include <qstringlist.h>

#include <klocale.h>
#include <kdebug.h>

using namespace CppSupport;
using namespace std;


#define ADVANCE(tk, descr) \
{ \
    Token token = lex->lookAhead( 0 ); \
    if( token != tk ){ \
        reportError( Errors::SyntaxError ); \
        return false; \
    } \
    lex->nextToken(); \
}

#define MATCH(tk, descr) \
{ \
    Token token = lex->lookAhead( 0 ); \
    if( token != tk ){ \
        reportError( Errors::SyntaxError ); \
        return false; \
    } \
}

Parser::Parser( ProblemReporter* pr, Driver* drv, Lexer* lexer )
    : m_problemReporter( pr ),
      driver( drv ),
      lex( lexer )
{
    m_fileName = "<stdin>";

    dom = new QDomDocument( "Cpp" );
    translationUnit = dom->createElement( "TranslationUnit" );
    dom->appendChild( translationUnit );

    m_maxErrors = 15;
    resetErrors();
}

Parser::~Parser()
{
    if( dom ){
        delete( dom );
        dom = 0;
    }
}

void Parser::setFileName( const QString& fileName )
{
    m_fileName = fileName;
}

bool Parser::reportError( const Error& err )
{
    if( m_errors < m_maxErrors ){
        int line=0, col=0;
        const Token& token = lex->lookAhead( 0 );
        lex->getTokenPosition( token, &line, &col );

        m_problemReporter->reportError( err.text.arg(lex->lookAhead(0).toString()),
                                        m_fileName,
                                        line,
                                        col );
    }

    ++m_errors;

    return true;
}

bool Parser::reportError( const QString& msg )
{
    if( m_errors < m_maxErrors ){
        int line=0, col=0;
        const Token& token = lex->lookAhead( 0 );
        lex->getTokenPosition( token, &line, &col );

        m_problemReporter->reportError( msg,
                                        m_fileName,
                                        line,
                                        col );
    }

    ++m_errors;

    return true;
}

void Parser::syntaxError()
{
    reportError( Errors::SyntaxError );
}

void Parser::parseError()
{
    reportError( Errors::ParseError );
}

bool Parser::skipUntil( int token )
{
    while( !lex->lookAhead(0).isNull() ){
        if( lex->lookAhead(0) == token )
            return true;

        lex->nextToken();
    }

    return false;
}

bool Parser::skipUntilDeclaration()
{
    while( !lex->lookAhead(0).isNull() ){
        switch( lex->lookAhead(0) ){
        case ';':
        case '~':
        case Token_scope:
        case Token_identifier:
        case Token_operator:
        case Token_char:
        case Token_wchar_t:
        case Token_bool:
        case Token_short:
        case Token_int:
        case Token_long:
        case Token_signed:
        case Token_unsigned:
        case Token_float:
        case Token_double:
        case Token_void:
        case Token_extern:
        case Token_namespace:
        case Token_using:
        case Token_typedef:
        case Token_asm:
        case Token_template:
        case Token_export:

        case Token_const:       // cv
        case Token_volatile:    // cv

        case Token_public:
        case Token_protected:
        case Token_private:
        case Token_signals:      // Qt
        case Token_slots:        // Qt
            return true;

        default:
            lex->nextToken();
        }
    }

    return false;
}

bool Parser::skip( int l, int r )
{
    int count = 0;
    while( !lex->lookAhead(0).isNull() ){
        int tk = lex->lookAhead( 0 );

        if( tk == l )
            ++count;
        else if( tk == r )
            --count;

        if( count == 0 )
            return true;

        lex->nextToken();
    }

    return false;
}

bool Parser::parseName()
{
    //kdDebug(9007) << "Parser::parseName()" << endl;

    if( lex->lookAhead(0) == Token_scope ){
        lex->nextToken();
    }

    if( parseNestedNameSpecifier() )
    	return parseUnqualiedName();
    return false;
}

bool Parser::parseTranslationUnit()
{
    //kdDebug(9007) << "Parser::parseTranslationUnit()" << endl;
    while( !lex->lookAhead(0).isNull() ){
        if( !parseDefinition() ){
            // error recovery
            syntaxError();
            int tk = lex->lookAhead( 0 );
            skipUntilDeclaration();
            if( lex->lookAhead(0) == tk ){
                lex->nextToken();
            }
        }
    }

    return m_errors == 0;
}

bool Parser::parseDefinition()
{
    //kdDebug(9007) << "Parser::parseDefinition()" << endl;
    switch( lex->lookAhead(0) ){

    case ';':
        lex->nextToken();
        return true;

    case Token_extern:
        return parseLinkageSpecification();

    case Token_namespace:
        return parseNamespace();

    case Token_using:
        return parseUsing();

    case Token_typedef:
        return parseTypedef();

    case Token_asm:
        return parseAsmDefinition();

    case Token_template:
    case Token_export:
        return parseTemplateDeclaration();

    default:
        return parseDeclaration();

    } // end switch
}


bool Parser::parseLinkageSpecification()
{
    //kdDebug(9007) << "Parser::parseLinkageSpecification()" << endl;

    if( lex->lookAhead(0) != Token_extern ){
        return false;
    }
    lex->nextToken();

    parseStringLiteral();

    if( lex->lookAhead(0) == '{' ){
        if( parseLinkageBody() ){
        }
    } else {
        if( !parseDefinition() ){
            reportError( i18n("Declaration syntax error") );
            return true;
        }
    }

    return true;
}

bool Parser::parseLinkageBody()
{
    //kdDebug(9007) << "Parser::parseLinkageBody()" << endl;
    if( lex->lookAhead(0) != '{' ){
        return false;
    }
    lex->nextToken();

    while( !lex->lookAhead(0).isNull() ){
        int tk = lex->lookAhead( 0 );

        if( tk == '}' )
            break;

        if( !parseDefinition() ){
            // error recovery
            syntaxError();
            if( skipUntil(';') ){
                lex->nextToken(); // skip ;
            }
        } else {
        }
    }

    if( lex->lookAhead(0) != '}' ){
        reportError( i18n("} expected") );
        skipUntil( '}' );
    }
    lex->nextToken();

    return true;
}

bool Parser::parseNamespace()
{
    //kdDebug(9007) << "Parser::parseNamespace()" << endl;
    if( lex->lookAhead(0) != Token_namespace ){
        return false;
    }
    lex->nextToken();


    if( lex->lookAhead(0) == Token_identifier ){
        lex->nextToken();
    }

    if ( lex->lookAhead(0) == '=' ) {
        // namespace alias
        lex->nextToken();

        if( parseName() ){

            if( lex->lookAhead(0) != ';' ){
                reportError( i18n("; expected") );
                skipUntil( ';' );
            }
            lex->nextToken(); // skip ;
            return true;
        } else {
            reportError( i18n("namespace expected") );
            return false;
        }
    }

    if( lex->lookAhead(0) != '{' ){
        reportError( i18n("{ expected") );
        skipUntil( '{' );
    }

    if( parseLinkageBody() ){
    }

    return true;
}

bool Parser::parseUsing()
{
    //kdDebug(9007) << "Parser::parseUsing()" << endl;

    if( lex->lookAhead(0) != Token_using ){
        return false;
    }
    lex->nextToken();

    if( lex->lookAhead(0) == Token_namespace ){
        return parseUsingDirective();
    }


    if( parseName() ){

        if( lex->lookAhead(0) != ';' ){
            reportError( i18n("; expected") );
            skipUntil( ';' );
        }
        lex->nextToken(); // skip ;
        return true;
    }

    if( lex->lookAhead(0) == Token_typename ){
        lex->nextToken();
    }

    if( !parseName() ){
        reportError( i18n("Namespace name expected") );
        return false;
    }

    if( lex->lookAhead(0) != ';' ){
        reportError( i18n("; expected") );
        skipUntil( ';' );
    }
    lex->nextToken();

    return true;
}

bool Parser::parseUsingDirective()
{
    //kdDebug(9007) << "Parser::parseUsingDirective()" << endl;

    if( lex->lookAhead(0) != Token_namespace ){
        return false;
    }
    lex->nextToken();


    if( !parseName() ){
        reportError( i18n("Namespace name expected") );
    } else {
    }

    if( lex->lookAhead(0) != ';' ){
        reportError( i18n("} expected") );
        skipUntil( ';' );
    }
    lex->nextToken();

    return true;
}


bool Parser::parseOperatorFunctionId()
{
    //kdDebug(9007) << "Parser::parseOperatorFunctionId()" << endl;
    if( lex->lookAhead(0) != Token_operator ){
        return false;
    }
    lex->nextToken();


    if( parseOperator() )
        return true;
    else {
        // parse cast operator
        QStringList cv;
        if( parseCvQualify(cv) ){
        }

        if( !parseSimpleTypeSpecifier() ){
            parseError();
        }

        if( parseCvQualify(cv) ){
        }

        while( parsePtrOperator() ){
        }

        return true;
    }
}

bool Parser::parseTemplateArgumentList()
{
    //kdDebug(9007) << "Parser::parseTemplateArgumentList()" << endl;

    if( !parseTemplateArgument() )
        return false;

    while( lex->lookAhead(0) == ',' ){
        lex->nextToken();

        if( parseTemplateArgument() ){
        } else {
            parseError();
            break;
        }
    }

    return true;
}

bool Parser::parseTypedef()
{
    //kdDebug(9007) << "Parser::parseTypedef()" << endl;

    if( lex->lookAhead(0) != Token_typedef ){
        return false;
    }
    lex->nextToken();


    if( !parseTypeSpecifier() ){
        reportError( i18n("Need a type specifier to declare") );
    }

    if( !parseDeclarator() ){
        reportError( i18n("Need an identifier to declare") );
    }

    if( lex->lookAhead(0) != ';' ){
        reportError( i18n("} expected") );
        skipUntil( ';' );
    }
    lex->nextToken();


    return true;
}

bool Parser::parseAsmDefinition()
{
    //kdDebug(9007) << "Parser::parseAsmDefinition()" << endl;

    ADVANCE( Token_asm, "asm" );
    ADVANCE( '(', '(' );

    parseStringLiteral();

    ADVANCE( ')', ')' );
    ADVANCE( ';', ';' );

    return true;
}

bool Parser::parseTemplateDeclaration()
{
    //kdDebug(9007) << "Parser::parseTemplateDeclaration()" << endl;

    bool _export = false;
    if( lex->lookAhead(0) == Token_export ){
        _export = true;
        lex->nextToken();
    }

    if( lex->lookAhead(0) != Token_template ){
        if( _export ){
            ADVANCE( Token_template, "template" );
        } else
            return false;
    }

    ADVANCE( Token_template, "template" );

    if( lex->lookAhead(0) == '<' ){
        lex->nextToken();


        if( parseTemplateParameterList() ){
        }

        if( lex->lookAhead(0) != '>' ){
            reportError( i18n("> expected") );
            skipUntil( '>' );
        }
        lex->nextToken();
    }

    if( !parseDefinition( ) ){
        reportError( i18n("expected a declaration") );
    } else {
    }

    return true;
}

bool Parser::parseDeclaration()
{
    //kdDebug(9007) << "Parser::parseDeclaration()" << endl;

    return
        parseIntegralDeclaration() ||
        parseConstDeclaration() ||
        parseOtherDeclaration();
}

bool Parser::parseDeclHead()
{
    //kdDebug(9007) << "Parser::parseDeclHead()" << endl;

    while( parseStorageClassSpecifier() || parseFunctionSpecifier() ){
    }

    QStringList cv;
    if( parseCvQualify(cv) ){
    }

    return true;
}

bool Parser::parseIntegralDeclaration()
{
    //kdDebug(9007) << "Parser::parseIntegralDeclaration()" << endl;
    int index = lex->index();

    if( !parseIntegralDeclHead() ){
        lex->setIndex( index );
        return false;
    }

    int tk = lex->lookAhead( 0 );
    if( tk == ';' ){
        lex->nextToken();
    } else if( tk == ':' ){
        lex->nextToken();
        if( !parseConstantExpression() ){
            lex->setIndex( index );
            return false;
        }
        ADVANCE( ';', ';' );
    } else {
        if( !parseInitDeclaratorList() ){
            lex->setIndex( index );
            return false;
        }

        if( lex->lookAhead(0) == ';' ){
            lex->nextToken();
        } else {
            if( !parseFunctionBody() ){
                lex->setIndex( index );
                return false;
            }
        }
    }

    return true;
}

bool Parser::parseIntegralDeclHead()
{
    //kdDebug(9007) << "Parser::parseIntegralDeclHead()" << endl;

    if( !parseDeclHead() ){
        return false;
    }

    if( !parseTypeSpecifier() ){
        return false;
    }

    QStringList cv;
    parseCvQualify( cv );

    return true;
}

bool Parser::parseOtherDeclaration()
{
    //kdDebug(9007) << "Parser::parseOtherDeclaration()" << endl;

    if( lex->lookAhead(0) == Token_friend ){
        lex->nextToken();
        if( !parseDefinition() ){
            reportError( i18n("expected a declaration") );
            return false;
        }
    } else {

        if( !parseDeclHead() ){
            return false;
        }

        if( !parseName() ){
            return false;
        }

        if( isConstructorDecl() ){
            if( !parseConstructorDeclaration() ){
                return false;
            }
        } else {

            QStringList cv;
            if( parseCvQualify(cv) ){
            }

            if( !parseInitDeclaratorList() ){
                return false;
            }
        }

        if( lex->lookAhead(0) == ';' ){
            // constructor/destructor or cast operator
            lex->nextToken();
        } else {
            if( !parseFunctionBody() ){
                return false;
            }
        }
    }

    return true;
}

bool Parser::parseConstDeclaration()
{
    //kdDebug(9007) << "Parser::parseConstDeclaration()" << endl;

    if( lex->lookAhead(0) != Token_const ){
        return false;
    }
    lex->nextToken();

    int index = lex->index();

    if( !parseInitDeclaratorList() ){
        lex->setIndex( index );
        return false;
    }

    ADVANCE( ';', ';' );

    return true;
}

bool Parser::isConstructorDecl()
{
    //kdDebug(9007) << "Parser::isConstructorDecl()" << endl;

    if( lex->lookAhead(0) != '(' ){
        return false;
    }

    int tk = lex->lookAhead( 1 );
    switch( tk ){
    case '*':
    case '&':
    case '(':
        return false;

    case Token_const:
    case Token_volatile:
        return true;

    case Token_scope:
    {
        int index = lex->index();
        if( parsePtrToMember() )
            return false;

        lex->setIndex( index );
    }
    break;

    }

    return true;
}

bool Parser::parseConstructorDeclaration() // or castoperator declaration
{
    //kdDebug(9007) << "Parser::parseConstructorDeclaration()" << endl;

    if( lex->lookAhead(0) != '(' ){
        return false;
    }
    lex->nextToken();


    if( !parseParameterDeclarationClause() ){
        return false;
    }

    if( lex->lookAhead(0) != ')' ){
        return false;
    }
    lex->nextToken();

    if( lex->lookAhead(0) == Token_const ){
        lex->nextToken();
    }

    if( lex->lookAhead(0) == ':' ){
        if( !parseCtorInitializer() ){
            return false;
        }
    }

    return true;
}

bool Parser::parseOperator()
{
    //kdDebug(9007) << "Parser::parseOperator()" << endl;

    switch( lex->lookAhead(0) ){
    case Token_new:
    case Token_delete:
        lex->nextToken();
        if( lex->lookAhead(0) == '[' && lex->lookAhead(1) == ']' ){
            lex->nextToken();
            lex->nextToken();
        } else {
        }
        return true;

    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '^':
    case '&':
    case '|':
    case '~':
    case '!':
    case '=':
    case '<':
    case '>':
    case ',':
    case Token_assign:
    case Token_shift:
    case Token_eq:
    case Token_not_eq:
    case Token_leq:
    case Token_geq:
    case Token_and:
    case Token_or:
    case Token_incr:
    case Token_decr:
    case Token_ptrmem:
    case Token_arrow:
        lex->nextToken();
        return true;

    default:
        if( lex->lookAhead(0) == '(' && lex->lookAhead(1) == ')' ){
            lex->nextToken();
            lex->nextToken();
            return true;
        } else if( lex->lookAhead(0) == '[' && lex->lookAhead(1) == ']' ){
            lex->nextToken();
            lex->nextToken();
            return true;
        }
    }

    return false;
}

bool Parser::parseCvQualify( QStringList& l )
{
    //kdDebug(9007) << "Parser::parseCvQualify()" << endl;

    int n = 0;
    while( !lex->lookAhead(0).isNull() ){
        int tk = lex->lookAhead( 0 );
        if( tk == Token_const || tk == Token_volatile ){
            ++n;
            l << lex->lookAhead( 0 ).toString();
            lex->nextToken();
        } else
            break;
    }
    return n != 0;
}

bool Parser::parseSimpleTypeSpecifier()
{
    //kdDebug(9007) << "Parser::parseSimpleTypeSpecifier()" << endl;

    bool isIntegral = false;

    while( !lex->lookAhead(0).isNull() ){
        int tk = lex->lookAhead( 0 );

        if( tk == Token_char || tk == Token_wchar_t || tk == Token_bool || tk == Token_short ||
            tk == Token_int || tk == Token_long || tk == Token_signed || tk == Token_unsigned ||
            tk == Token_float || tk == Token_double || tk == Token_void ){

            isIntegral = true;
            lex->nextToken();
        } else if( isIntegral ){
            return true;
        } else
            break;
    }

    return parseName();
}

bool Parser::parsePtrOperator()
{
    //kdDebug(9007) << "Parser::parsePtrOperator()" << endl;

    if( lex->lookAhead(0) == '&' ){
        lex->nextToken();
    } else if( lex->lookAhead(0) == '*' ){
        lex->nextToken();
    } else {
        int index = lex->index();
        if( !parsePtrToMember() ){
            lex->setIndex( index );
            return false;
        }
    }

    QStringList cv;
    if( parseCvQualify(cv) ){
    }

    return true;
}


bool Parser::parseTemplateArgument()
{
    //kdDebug(9007) << "Parser::parseTemplateArgument()" << endl;

#warning "TODO Parser::parseTemplateArgument()"
#warning "parse type id"

#if 0
    if( parseTypeId() ){
        qWarning( "token = %s", lex->lookAhead(0).toString().latin1() );
        return true;
    }
#endif

    return parseAssignmentExpression();
}

bool Parser::parseTypeSpecifier()
{
    //kdDebug(9007) << "Parser::parseTypeSpecifier()" << endl;

    QStringList cv;
    parseCvQualify(cv);

    if( parseClassSpecifier() || parseEnumSpecifier() ||
        parseElaboratedTypeSpecifier() || parseSimpleTypeSpecifier() ){

        if( parseCvQualify(cv) ){
        }
    } else
        return false;

    return true;
}

bool Parser::parseDeclarator()
{
    //kdDebug(9007) << "Parser::parseDeclarator()" << endl;

    while( parsePtrOperator() ){
    }

    if( lex->lookAhead(0) == '(' ){
        lex->nextToken();

        if( !parseDeclarator() ){
//             reportError( "expected a nested declarator",
//                          lex->lookAhead(0) );
            return false;
        }
        if( lex->lookAhead(0) != ')'){
            return false;
        }
        lex->nextToken();
    } else {
        if( !parseDeclaratorId() ){
            return false;
        }

        if( lex->lookAhead(0) == ':' ){
            lex->nextToken();
            if( !parseConstantExpression() ){
                reportError( i18n("Constant expression expected") );
                return true;
            } else {
            }
            return true;
        }

    }

    while( lex->lookAhead(0) == '[' ){
        lex->nextToken();
        if( !parseCommaExpression() ){
            reportError( i18n("Expression expected") );
        } else {
        }

        if( lex->lookAhead(0) != ']' ){
            reportError( i18n("] expected") );
            skipUntil( ']' );
        }
        lex->nextToken();
    }

    if( lex->lookAhead(0) == '(' ){
        // parse function arguments
        lex->nextToken();

        if( !parseParameterDeclarationClause() ){
            return false;
        }
        if( lex->lookAhead(0) != ')' ){
            reportError( i18n(") expected") );
            skipUntil( ')' );
        }
        lex->nextToken();

        QStringList cv;
        if( parseCvQualify(cv) ){
        }

        if( parseExceptionSpecification() ){
        }
    }

    return true;
}

bool Parser::parseEnumSpecifier()
{
    //kdDebug(9007) << "Parser::parseEnumSpecifier()" << endl;

    int index = lex->index();

    if( lex->lookAhead(0) != Token_enum ){
        return false;
    }

    lex->nextToken();

    if( lex->lookAhead(0) == Token_identifier ){
        lex->nextToken();
    }

    if( lex->lookAhead(0) != '{' ){
        lex->setIndex( index );
        return false;
    }

    lex->nextToken();

    if( parseEnumeratorList() ){
    }

    if( lex->lookAhead(0) != '}' ){
        reportError( i18n("} expected") );
        skipUntil( '}' );
    }
    lex->nextToken();

    return true;
}

bool Parser::parseTemplateParameterList()
{
    //kdDebug(9007) << "Parser::parseTemplateParameterList()" << endl;

    if( !parseTemplateParameter() ){
        return false;
    }

    while( lex->lookAhead(0) == ',' ){
        lex->nextToken();

        if( parseTemplateParameter() ){
        } else {
            parseError();
            break;
        }
    }

    return true;
}

bool Parser::parseTemplateParameter()
{
    //kdDebug(9007) << "Parser::parseTemplateParameter()" << endl;

    int tk = lex->lookAhead( 0 );
    if( tk == Token_class ||
        tk == Token_typename ||
        tk == Token_template )
        return parseTypeParameter();
    else
        return parseParameterDeclaration();
}

bool Parser::parseTypeParameter()
{
    //kdDebug(9007) << "Parser::parseTypeParameter()" << endl;

    switch( lex->lookAhead(0) ){

    case Token_class:
    {

        lex->nextToken(); // skip class

        // parse optional name
        QString name;
        if( lex->lookAhead(0) == Token_identifier ){
            name = lex->lookAhead( 0 ).toString();
            lex->nextToken();
        }
        if( name )

        if( lex->lookAhead(0) == '=' ){
            lex->nextToken();

            if( !parseTypeId() ){
                parseError();
            }
        }
    }
    return true;

    case Token_typename:
    {

        lex->nextToken(); // skip typename

        // parse optional name
        QString name;
        if( lex->lookAhead(0) == Token_identifier ){
            name = lex->lookAhead( 0 ).toString();
            lex->nextToken();
        }
        if( name )

        if( lex->lookAhead(0) == '=' ){
            lex->nextToken();

            if( !parseTypeId() ){
                parseError();
            }
        }
    }
    return true;

    case Token_template:
    {

        lex->nextToken(); // skip template
        ADVANCE( '<', '<' );
        if( !parseTemplateParameterList() ){
            return false;
        }

        ADVANCE( '>', '>' );
        ADVANCE( Token_class, "class" );

        // parse optional name
        QString name;
        if( lex->lookAhead(0) == Token_identifier ){
            name = lex->lookAhead( 0 ).toString();
            lex->nextToken();
        }
        if( name )

        if( lex->lookAhead(0) == '=' ){
            lex->nextToken();

            QString templ_name = lex->lookAhead( 0 ).toString();
            if( lex->lookAhead(0) != Token_identifier ){
                reportError( i18n("Expected an identifier") );
            } else {
                lex->nextToken(); // skip template-name
            }
        }
    }
    return true;

    } // end switch


    return false;
}

bool Parser::parseStorageClassSpecifier()
{
    //kdDebug(9007) << "Parser::parseStorageClassSpecifier()" << endl;

    switch( lex->lookAhead(0) ){
    case Token_friend:
    case Token_auto:
    case Token_register:
    case Token_static:
    case Token_extern:
    case Token_mutable:
        lex->nextToken();
        return true;
    }

    return false;
}

bool Parser::parseFunctionSpecifier()
{
    //kdDebug(9007) << "Parser::parseFunctionSpecifier()" << endl;

    switch( lex->lookAhead(0) ){
    case Token_inline:
    case Token_virtual:
    case Token_explicit:
        lex->nextToken();
        return true;
    }

    return false;
}

bool Parser::parseTypeId()
{
    //kdDebug(9007) << "Parser::parseTypeId()" << endl;

    if( !parseTypeSpecifier() ){
        return false;
    }

    if( parseAbstractDeclarator() ){
    }

    return true;
}

bool Parser::parseAbstractDeclarator()
{
    //kdDebug(9007) << "Parser::parseAbstractDeclarator()" << endl;

#warning "TODO Parser::parseAbstractDeclarator()"
#warning "resolve ambiguities between sub-abstract-declarator and function argument"

    while( parsePtrOperator() ){
    }

    if( lex->lookAhead(0) == '(' ){
        lex->nextToken();

        if( !parseDeclarator() ){
//             reportError( "expected a nested declarator",
//                          lex->lookAhead(0) );
            return false;
        }
        if( lex->lookAhead(0) != ')'){
            reportError( i18n(") expected") );
            skipUntil( ')' );
        }
        lex->nextToken();
    }

    while( lex->lookAhead(0) == '[' ){
        lex->nextToken();
        if( !parseCommaExpression() ){
            reportError( i18n("Expression expected") );
            return false;
        }
        if( lex->lookAhead(0) != ']' ){
            reportError( i18n("] expected") );
            skipUntil( ']' );
            return false;
        }
        lex->nextToken();
    }

    if( lex->lookAhead(0) == '(' ){
        // parse function arguments
        lex->nextToken();

        if( !parseParameterDeclarationClause() ){
            return false;
        }
        if( lex->lookAhead(0) != ')' ){
            reportError( i18n(") expected") );
            skipUntil( ')' );
        }
        lex->nextToken();


        QStringList cv;
        if( parseCvQualify(cv) ){
        }

        if( parseExceptionSpecification() ){
        }
    }

    return true;
}

bool Parser::parseConstantExpression()
{
    //kdDebug(9007) << "Parser::parseConstantExpression()" << endl;

    QStringList l;

    while( !lex->lookAhead(0).isNull() ){
        int tk = lex->lookAhead( 0 );

        if( tk == '(' ){
            l << "(...)";
            if( !skip('(', ')') ){
                return false;
            }
            lex->nextToken();
        } else if( tk == ',' || tk == ';' || tk == '<' ||
                   tk == Token_assign || tk == ']' ||
                   tk == ')' || tk == '}' || tk == ':' ){
            break;
        } else {
            l << lex->lookAhead( 0 ).toString();
            lex->nextToken();
        }
    }

    return !l.isEmpty();
}


bool Parser::parseInitDeclaratorList()
{
    //kdDebug(9007) << "Parser::parseInitDeclaratorList()" << endl;

    if( !parseInitDeclarator() ){
        return false;
    }

    while( lex->lookAhead(0) == ',' ){
        lex->nextToken();

        if( parseInitDeclarator() ){
        } else {
            parseError();
            break;
        }
    }
    return true;
}

bool Parser::parseFunctionBody()
{
    //kdDebug(9007) << "Parser::parseFunctionBody()" << endl;

    if( lex->lookAhead(0) != '{' ){
        return false;
    }

    return parseCompoundStatement();
}

bool Parser::parseParameterDeclarationClause()
{
    //kdDebug(9007) << "Parser::parseParameterDeclarationClause()" << endl;

    if( parseParameterDeclarationList() ){
    }

    if( lex->lookAhead(0) == ',' ){
        lex->nextToken();
    }

    if( lex->lookAhead(0) == Token_ellipsis ){
        lex->nextToken();
    }

    return true;
}

bool Parser::parseParameterDeclarationList()
{
    //kdDebug(9007) << "Parser::parseParameterDeclarationList()" << endl;

    if( !parseParameterDeclaration() ){
        return false;
    }

    while( lex->lookAhead(0) == ',' ){
        lex->nextToken();

        if( parseParameterDeclaration() ){
        } else {
            break;
        }
    }
    return true;
}

bool Parser::parseParameterDeclaration()
{
    //kdDebug(9007) << "Parser::parseParameterDeclaration()" << endl;

    // parse decl spec
    if( !parseTypeSpecifier() ){
        return false;
    }

    int index = lex->index();
    if( parseDeclarator() ){
    } else {
        lex->setIndex( index );
        // optional abstract declarator
        if( parseAbstractDeclarator() ){
        }
    }

    if( lex->lookAhead(0) == '=' ){
        lex->nextToken();
        if( !parseAssignmentExpression() ){
            reportError( i18n("Expression expected") );
        }
    }

    return true;
}

bool Parser::parseClassSpecifier()
{
    //kdDebug(9007) << "Parser::parseClassSpecifier()" << endl;

    int index = lex->index();

    int kind = lex->lookAhead( 0 );
    if( kind == Token_class || kind == Token_struct || kind == Token_union ){
        lex->nextToken();
    } else {
        return false;
    }

    if( lex->lookAhead(0) == Token_identifier ){
        lex->nextToken();
    }

    if( parseBaseClause() ){
    }

    if( lex->lookAhead(0) != '{' ){
        lex->setIndex( index );
        return false;
    }

    ADVANCE( '{', '{' );

    if( lex->lookAhead(0) != '}' ){
        if( parseMemberSpecificationList() ){
        }
    }

    if( lex->lookAhead(0) != '}' ){
        reportError( i18n("} expected") );
        skipUntil( '}' );
    }
    lex->nextToken();

    return true;
}

bool Parser::parseAccessSpecifier()
{
    //kdDebug(9007) << "Parser::parseAccessSpecifier()" << endl;

    switch( lex->lookAhead(0) ){
    case Token_public:
    case Token_protected:
    case Token_private:
        lex->nextToken();
        return true;
    }

    return false;
}

bool Parser::parseMemberSpecificationList()
{
    //kdDebug(9007) << "Parser::parseMemberSpecificationList()" << endl;

    if( !parseMemberSpecification() ){
        return false;
    }

    while( !lex->lookAhead(0).isNull() ){
        if( lex->lookAhead(0) == '}' )
            break;

        if( !parseMemberSpecification() ){
            int tk = lex->lookAhead( 0 );
            skipUntilDeclaration();
            if( lex->lookAhead(0) == tk ){
                lex->nextToken();
            }
        }
    }

    return true;
}

bool Parser::parseMemberSpecification()
{
    //kdDebug(9007) << "Parser::parseMemberSpecification()" << endl;

    if( lex->lookAhead(0) == ';' ){
        lex->nextToken();
        return true;
    } else if( lex->lookAhead(0) == Token_Q_OBJECT || lex->lookAhead(0) == Token_K_DCOP ){
        lex->nextToken();
        return true;
    } else if( lex->lookAhead(0) == Token_signals || lex->lookAhead(0) == Token_k_dcop ){
        lex->nextToken();
        if( lex->lookAhead(0) != ':' ){
            reportError( i18n(": expected") );
        } else
            lex->nextToken();
        return true;
    } else if( parseTypedef() ){
        return true;
    } else if( parseUsing() ){
        return true;
    } else if( parseTemplateDeclaration() ){
        return true;
    } else if( parseAccessSpecifier() ){
        if( lex->lookAhead(0) == Token_slots ){
            lex->nextToken();
        }
        if( lex->lookAhead(0) != ':' ){
            reportError( i18n(": expected") );
            return false;
        } else
            lex->nextToken();
        return true;
    }

    return parseDeclaration();
}

bool Parser::parseCtorInitializer()
{
    //kdDebug(9007) << "Parser::parseCtorInitializer()" << endl;

    if( lex->lookAhead(0) != ':' ){
        return false;
    }
    lex->nextToken();

    if( !parseMemInitializerList() ){
        reportError( i18n("Member initializers expected") );
    }

    return true;
}

bool Parser::parseElaboratedTypeSpecifier()
{
    //kdDebug(9007) << "Parser::parseElaboratedTypeSpecifier()" << endl;

    int index = lex->index();

    int tk = lex->lookAhead( 0 );
    if( tk == Token_class  ||
        tk == Token_struct ||
        tk == Token_union  ||
        tk == Token_enum   ||
        tk == Token_typename )
    {
        lex->nextToken();

        if( parseName() ){
            return true;
        }
    }

    lex->setIndex( index );
    return false;
}

bool Parser::parseDeclaratorId()
{
    //kdDebug(9007) << "Parser::parseDeclaratorId()" << endl;
    return parseName();
}

bool Parser::parseExceptionSpecification()
{
    //kdDebug(9007) << "Parser::parseExceptionSpecification()" << endl;

    if( lex->lookAhead(0) != Token_throw ){
        return false;
    }
    lex->nextToken();


    ADVANCE( '(', '(' );
    if( !parseTypeIdList() ){
        reportError( i18n("Type id list expected") );
        return false;
    }

    if( lex->lookAhead(0) != ')' ){
        reportError( i18n(") expected") );
        skipUntil( ')' );
    }
    lex->nextToken();

    return true;
}

bool Parser::parseEnumeratorList()
{
    //kdDebug(9007) << "Parser::parseEnumeratorList()" << endl;

    if( !parseEnumerator() ){
        return false;
    }

    while( lex->lookAhead(0) == ',' ){
        lex->nextToken();

        if( parseEnumerator() ){
        } else {
            reportError( i18n("Enumerator expected") );
            break;
        }
    }

    return true;
}

bool Parser::parseEnumerator()
{
    //kdDebug(9007) << "Parser::parseEnumerator()" << endl;

    if( lex->lookAhead(0) != Token_identifier ){
        return false;
    }
    lex->nextToken();

    if( lex->lookAhead(0) == '=' ){
        lex->nextToken();

        if( !parseConstantExpression() ){
            reportError( i18n("Constant expression expected") );
        }
    }

    return true;
}

bool Parser::parseInitDeclarator()
{
    //kdDebug(9007) << "Parser::parseInitDeclarator()" << endl;

    if( !parseDeclarator() ){
        return false;
    }

    if( parseInitializer() ){
    }

    return true;
}

bool Parser::parseAssignmentExpression()
{
    //kdDebug(9007) << "Parser::parseAssignmentExpression()" << endl;

#warning "TODO Parser::parseAssignmentExpression()"

    while( !lex->lookAhead(0).isNull() ){
        int tk = lex->lookAhead( 0 );

        if( tk == '(' ){
            if( !skip('(', ')') ){
                return false;
            } else
                lex->nextToken();
        } else if( tk == '<' ){
            if( !skip('<', '>') ){
                return false;
            } else
                lex->nextToken();
        } else if( tk == '[' ){
            if( !skip('[', ']') ){
                return false;
            } else
                lex->nextToken();
        } else if( tk == ',' || tk == ';' ||
                   tk == '>' || tk == ']' || tk == ')' ||
                   tk == Token_assign ){
            break;
        } else
            lex->nextToken();
    }

    return true;
}

bool Parser::parseBaseClause()
{
    //kdDebug(9007) << "Parser::parseBaseClause()" << endl;

    if( lex->lookAhead(0) != ':' ){
        return false;
    }
    lex->nextToken();


    if( !parseBaseSpecifierList() ){
        reportError( i18n("expected base specifier list") );
        return false;
    }

    return true;
}

bool Parser::parseInitializer()
{
    //kdDebug(9007) << "Parser::parseInitializer()" << endl;

    if( lex->lookAhead(0) == '=' ){
        lex->nextToken();

        if( !parseInitializerClause() ){
            reportError( i18n("Initializer clause expected") );
            return false;
        }
    } else if( lex->lookAhead(0) == '(' ){
        lex->nextToken();
        if( !parseCommaExpression() ){
            reportError( i18n("Expression expected") );
            return false;
        }
        if( lex->lookAhead(0) != ')' ){
            reportError( i18n(") expected") );
            skipUntil( ')' );
        }
        lex->nextToken();
    } else
        return false;

    return true;
}

bool Parser::parseMemInitializerList()
{
    //kdDebug(9007) << "Parser::parseMemInitializerList()" << endl;

    if( !parseMemInitializer() ){
        return false;
    }

    while( lex->lookAhead(0) == ',' ){
        lex->nextToken();

        if( parseMemInitializer() ){
        } else {
            break;
        }
    }

    return true;
}

bool Parser::parseMemInitializer()
{
    //kdDebug(9007) << "Parser::parseMemInitializer()" << endl;

    if( !parseMemInitializerId() ){
        reportError( i18n("Identifier expected") );
        return false;
    }
    ADVANCE( '(', '(' );
    if( !parseCommaExpression() ){
        reportError( i18n("Expression expected") );
        return false;
    }

    if( lex->lookAhead(0) != ')' ){
        reportError( i18n(") expected") );
        skipUntil( ')' );
    }
    lex->nextToken();

    return true;
}

bool Parser::parseTypeIdList()
{
    //kdDebug(9007) << "Parser::parseTypeIdList()" << endl;

    if( !parseTypeId() ){
        return false;
    }

    while( lex->lookAhead(0) == ',' ){
        if( parseTypeId() ){
        } else {
            reportError( i18n("Type id expected") );
            break;
        }
    }

    return true;
}

bool Parser::parseBaseSpecifierList()
{
    //kdDebug(9007) << "Parser::parseBaseSpecifierList()" << endl;

    if( !parseBaseSpecifier() ){
        return false;
    }

    while( lex->lookAhead(0) == ',' ){
        lex->nextToken();

        if( parseBaseSpecifier() ){
        } else {
            reportError( i18n("Base class specifier expected") );
            break;
        }
    }

    return true;
}

bool Parser::parseBaseSpecifier()
{
    //kdDebug(9007) << "Parser::parseBaseSpecifier()" << endl;

    if( lex->lookAhead(0) == Token_virtual ){
        lex->nextToken();

        if( parseAccessSpecifier() ){
        }
    } else {

        if( parseAccessSpecifier() ){
        }

        if( lex->lookAhead(0) == Token_virtual ){
            lex->nextToken();
        }
    }

    if( lex->lookAhead(0) == Token_scope ){
        lex->nextToken();
    }

    if( !parseName() ){
        reportError( i18n("Identifier expected") );
    }

    return true;
}


bool Parser::parseInitializerClause()
{
    //kdDebug(9007) << "Parser::parseInitializerClause()" << endl;

#warning "TODO Parser::initializer-list"

    if( lex->lookAhead(0) == '{' ){
        if( !skip('{','}') ){
            reportError( i18n("} missing") );
        } else
            lex->nextToken();
    } else {
        if( !parseAssignmentExpression() ){
            reportError( i18n("Expression expected") );
        }
    }

    return true;
}

bool Parser::parseMemInitializerId()
{
    //kdDebug(9007) << "Parser::parseMemInitializerId()" << endl;

    return parseName();
}


bool Parser::parseCommaExpression()
{
    //kdDebug(9007) << "Parser::parseCommaExpression()" << endl;

    if( !parseExpression() )
    	return false;

    while( lex->lookAhead(0) == ',' ){
    	lex->nextToken();

    	if( !parseExpression() ){
            reportError( i18n("expression expected") );
            return false;
        }
    }
    return true;
}

bool Parser::parseNestedNameSpecifier()
{
    //kdDebug(9007) << "Parser::parseNestedNameSpecifier()" << endl;

    int index = lex->index();

    if( lex->lookAhead(0) != Token_scope &&
        lex->lookAhead(0) != Token_identifier ){
        return false;
    }

    while( lex->lookAhead(0) == Token_identifier ){

        if( lex->lookAhead(1) == '<' ){
            lex->nextToken(); // skip template name
            lex->nextToken(); // skip <

            if( parseTemplateArgumentList() ){
            }

            if( lex->lookAhead(0) != '>' ){
            	lex->setIndex( index );
            	return false;
            }
            lex->nextToken(); // skip >

            if ( lex->lookAhead(0) == Token_scope ) {
                lex->nextToken();
            } else
                return true;

        } else if( lex->lookAhead(1) == Token_scope ){
            lex->nextToken(); // skip name
            lex->nextToken(); // skip name
        } else
            break;
    }

    return true;
}

bool Parser::parsePtrToMember()
{
    //kdDebug(9007) << "Parser::parsePtrToMember()" << endl;

    if( lex->lookAhead(0) == Token_scope ){
        lex->nextToken();
    }

    while( lex->lookAhead(0) == Token_identifier ){
        lex->nextToken();

        if( lex->lookAhead(0) == Token_scope && lex->lookAhead(1) == '*' ){
            lex->nextToken(); // skip scope
            lex->nextToken(); // skip *
            return true;
        } else
            break;
    }

    return false;
}

bool Parser::parseQualifiedName()
{
    //kdDebug(9007) << "Parser::parseQualifiedName()" << endl;
    parseNestedNameSpecifier();

    if( lex->lookAhead(0) == Token_template ){
        lex->nextToken();
    }

    return parseUnqualiedName();
}

bool Parser::parseUnqualiedName()
{
    //kdDebug(9007) << "Parser::parseUnqualiedName()" << endl;

    bool isDestructor = false;

    if( lex->lookAhead(0) == Token_identifier ){
        lex->nextToken();
    } else if( lex->lookAhead(0) == '~' && lex->lookAhead(1) == Token_identifier ){
        lex->nextToken(); // skip ~
        lex->nextToken(); // skip classname
        isDestructor = true;
    } else if( lex->lookAhead(0) == Token_operator ){
        return parseOperatorFunctionId();
    } else
        return false;

    if( !isDestructor ){

        if( lex->lookAhead(0) == '<' ){
            lex->nextToken();

            // optional template arguments
            if( parseTemplateArgumentList() ){
            }

            if( lex->lookAhead(0) != '>' ){
                reportError( i18n("> expected") );
                skipUntil( '>' );
            }
            lex->nextToken();
        }
    }

    return true;
}

void Parser::dump()
{
    //kdDebug(9007) << dom->toString() << endl;
}

bool Parser::parseStringLiteral()
{
    while( !lex->lookAhead(0).isNull() ) {
        if( lex->lookAhead(0) == Token_identifier &&
            lex->lookAhead(0).toString() == "L" &&
            lex->lookAhead(1) == Token_string_literal ) {

            lex->nextToken();
            lex->nextToken();
        } else if( lex->lookAhead(0) == Token_string_literal ) {
            lex->nextToken();
        } else
            return false;
    }
    return true;
}

bool Parser::parseExpression()
{
    //kdDebug(9007) << "Parser::parseExpression()" << endl;

    bool ok = false;
    while( !lex->lookAhead(0).isNull() ){
        int tk = lex->lookAhead( 0 );

        if( tk == '(' ){
            if( !skip('(', ')') ){
                return false;
            } else
                lex->nextToken();
        } else if( tk == '[' ){
            if( !skip('[', ']') ){
                return false;
            } else
                lex->nextToken();
        } else if( tk == ';' || tk == ',' ||
                   tk == ']' || tk == ')' ){
            break;
        } else
            lex->nextToken();

        ok = true;
    }

    return ok;
}


bool Parser::parseExpressionStatement()
{
    //kdDebug(9007) << "Parser::parseExpressionStatement()" << endl;
    parseCommaExpression();
    if( lex->lookAhead(0) != ';' ){
    	reportError( i18n("; expected") );
        skipUntil( ';' );
    }
    lex->nextToken(); // skip ;

    return true;
}

bool Parser::parseStatement() // thanks to fiore@8080.it ;-)
{
    //kdDebug(9007) << "Parser::parseStatement()" << endl;
    switch( lex->lookAhead(0) ){

    case Token_while:
        return parseWhileStatement();

    case Token_do:
        return parseDoStatement();

    case Token_for:
        return parseForStatement();

    case Token_if:
        return parseIfStatement();

    case Token_switch:
        return parseSwitchStatement();

    case Token_case:
    case Token_default:
        return parseLabeledStatement();

    case Token_break:
    case Token_continue:
        lex->nextToken();
        ADVANCE( ';', ";" );
        return true;

    case Token_goto:
        lex->nextToken();
        ADVANCE( Token_identifier, "identifier" );
        ADVANCE( ';', ";" );
        return true;

    case Token_return:
        lex->nextToken();
        parseCommaExpression();
        ADVANCE( ';', ";" );
        return true;

    case '{':
        return parseCompoundStatement();

    case Token_identifier:
        if( parseLabeledStatement() )
            return true;
        break;
    }

    return parseExpressionStatement();
}

bool Parser::parseCondition()
{
    //kdDebug(9007) << "Parser::parseCondition()" << endl;

    int index = lex->index();
    if( parseTypeSpecifier() ){
    	if( parseDeclarator() && lex->lookAhead(0) == '=' ) {
            lex->nextToken();

            if( !parseAssignmentExpression() ){
                reportError( i18n("expression expected") );
            }
            return true;
        }
        lex->setIndex( index );
    }

    return parseCommaExpression();
}


bool Parser::parseWhileStatement()
{
    //kdDebug(9007) << "Parser::parseWhileStatement()" << endl;
    ADVANCE( Token_while, "while" );
    ADVANCE( '(' , "(" );
    if( !parseCondition() ){
    	reportError( i18n("condition expected") );
        skipUntil( ')' );
    }
    ADVANCE( ')', ")" );

    if( !parseStatement() ){
    	reportError( i18n("statement expected") );
        // TODO: skipUntilStatement();
    }

    return true;
}

bool Parser::parseDoStatement()
{
    //kdDebug(9007) << "Parser::parseDoStatement()" << endl;
    ADVANCE( Token_do, "do" );
    if( !parseStatement() ){
    	reportError( i18n("statement expected") );
        // TODO: skipUntilStatement();
    }
    ADVANCE( Token_while, "while" );
    ADVANCE( '(' , "(" );
    if( !parseCommaExpression() ){
    	reportError( i18n("expression expected") );
        skipUntil( ')' );
    }
    ADVANCE( ')', ")" );
    ADVANCE( ';', ";" );

    return true;
}

bool Parser::parseForStatement()
{
    //kdDebug(9007) << "Parser::parseForStatement()" << endl;
    ADVANCE( Token_for, "for" );
    ADVANCE( '(', "(" );

    //kdDebug(9007) << "1 token = " << lex->lookAhead(0).toString() << endl;
    if( !parseForInitStatement() ){
    	reportError( i18n("for initialization expected") );
    }
    //kdDebug(9007) << "2 token = " << lex->lookAhead(0).toString() << endl;

    parseCondition();
    ADVANCE( ';', ";" );
    parseCommaExpression();
    ADVANCE( ')', ")" );

    return parseStatement();
}

bool Parser::parseForInitStatement()
{
    //kdDebug(9007) << "Parser::parseForInitStatement()" << endl;
    return parseExpressionStatement();
}

bool Parser::parseCompoundStatement()
{
    //kdDebug(9007) << "Parser::parseCompoundStatement()" << endl;
    if( lex->lookAhead(0) != '{' ){
    	return false;
    }
    lex->nextToken();

    while( !lex->lookAhead(0).isNull() ){
        if( lex->lookAhead(0) == '}' )
            break;

    	if( !parseStatement() )
            break;
    }

    if( lex->lookAhead(0) != '}' ){
    	reportError( i18n("} expected") );
        skipUntil( '}' );
    }
    lex->nextToken();
    return true;
}

bool Parser::parseIfStatement()
{
    //kdDebug(9007) << "Parser::parseIfStatement()" << endl;

    ADVANCE( Token_if, "if" );

    ADVANCE( '(' , "(" );
    if( !parseCondition() ){
    	reportError( i18n("condition expected") );
        skipUntil( ')' );
    }
    ADVANCE( ')', ")" );

    if( !parseStatement() ){
    	reportError( i18n("statement expected") );
        // TODO: skipUntilStatement();
    }

    while( lex->lookAhead(0) == Token_else ){
    	lex->nextToken();
        if( !parseStatement() ) {
            reportError( i18n("statement expected") );
            return false;
        }
    }

    return true;
}

bool Parser::parseSwitchStatement()
{
    //kdDebug(9007) << "Parser::parseSwitchStatement()" << endl;
    ADVANCE( Token_switch, "switch" );

    ADVANCE( '(' , "(" );
    if( !parseCondition() ){
    	reportError( i18n("condition expected") );
        skipUntil( ')' );
    }
    ADVANCE( ')', ")" );

    return parseStatement();
}

bool Parser::parseLabeledStatement()
{
    //kdDebug(9007) << "Parser::parseLabeledStatement()" << endl;
    switch( lex->lookAhead(0) ){
    case Token_identifier:
    case Token_default:
        if( lex->lookAhead(1) == ':' ){
            lex->nextToken();
            lex->nextToken();
            return parseStatement();
        }
        break;

    case Token_case:
        lex->nextToken();
        if( !parseConstantExpression() ){
            reportError( i18n("expression expected") );
            skipUntil( ':' );
        }
        ADVANCE( ':', ":" );
        return parseStatement();
    }
    return false;
}

