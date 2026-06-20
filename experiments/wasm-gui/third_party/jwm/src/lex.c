/**
 * @file lex.c
 * @author Joe Wingbermuehle
 * @date 2004-2014
 *
 * @brief XML lexer functions.
 *
 */

#include "jwm.h"
#include "lex.h"
#include "error.h"
#include "misc.h"

/** Amount to increase allocations by when reading text. */
static const int BLOCK_SIZE = 16;

/** Mapping between token names and tokens.
 * These must be sorted.
 */
static const StringMappingType TOKEN_MAP[] = {
   { "Active",               TOK_ACTIVE               },
   { "Background",           TOK_BACKGROUND           },
   { "Button",               TOK_BUTTON               },
   { "ButtonClose",          TOK_BUTTONCLOSE          },
   { "ButtonCloseFocus",     TOK_BUTTONCLOSEFOCUS     },
   { "ButtonMax",            TOK_BUTTONMAX            },
   { "ButtonMaxActive",      TOK_BUTTONMAXACTIVE      },
   { "ButtonMaxActiveFocus", TOK_BUTTONMAXACTIVEFOCUS },
   { "ButtonMaxFocus",       TOK_BUTTONMAXFOCUS       },
   { "ButtonMenu",           TOK_BUTTONMENU           },
   { "ButtonMenuFocus",      TOK_BUTTONMENUFOCUS      },
   { "ButtonMin",            TOK_BUTTONMIN            },
   { "ButtonMinFocus",       TOK_BUTTONMINFOCUS       },
   { "Class",                TOK_CLASS                },
   { "Clock",                TOK_CLOCK                },
   { "ClockStyle",           TOK_CLOCKSTYLE           },
   { "Close",                TOK_CLOSE                },
   { "Corner",               TOK_CORNER               },
   { "DefaultIcon",          TOK_DEFAULTICON          },
   { "Desktop",              TOK_DESKTOP              },
   { "Desktops",             TOK_DESKTOPS             },
   { "Dock",                 TOK_DOCK                 },
   { "DoubleClickDelta",     TOK_DOUBLECLICKDELTA     },
   { "DoubleClickSpeed",     TOK_DOUBLECLICKSPEED     },
   { "Dynamic",              TOK_DYNAMIC              },
   { "Exit",                 TOK_EXIT                 },
   { "FocusModel",           TOK_FOCUSMODEL           },
   { "Font",                 TOK_FONT                 },
   { "Foreground",           TOK_FOREGROUND           },
   { "Group",                TOK_GROUP                },
   { "Height",               TOK_HEIGHT               },
   { "IconPath",             TOK_ICONPATH             },
   { "Include",              TOK_INCLUDE              },
   { "JWM",                  TOK_JWM                  },
   { "Key",                  TOK_KEY                  },
   { "Kill",                 TOK_KILL                 },
   { "Layer",                TOK_LAYER                },
   { "Machine",              TOK_MACHINE              },
   { "Maximize",             TOK_MAXIMIZE             },
   { "Menu",                 TOK_MENU                 },
   { "MenuStyle",            TOK_MENUSTYLE            },
   { "Minimize",             TOK_MINIMIZE             },
   { "Minimized",            TOK_MINIMIZED            },
   { "Mouse",                TOK_MOUSE                },
   { "Move",                 TOK_MOVE                 },
   { "MoveMode",             TOK_MOVEMODE             },
   { "Name",                 TOK_NAME                 },
   { "Opacity",              TOK_OPACITY              },
   { "Option",               TOK_OPTION               },
   { "Outline",              TOK_OUTLINE              },
   { "Pager",                TOK_PAGER                },
   { "PagerStyle",           TOK_PAGERSTYLE           },
   { "Popup",                TOK_POPUP                },
   { "PopupStyle",           TOK_POPUPSTYLE           },
   { "Program",              TOK_PROGRAM              },
   { "Resize",               TOK_RESIZE               },
   { "ResizeMode",           TOK_RESIZEMODE           },
   { "Restart",              TOK_RESTART              },
   { "RestartCommand",       TOK_RESTARTCOMMAND       },
   { "RootMenu",             TOK_ROOTMENU             },
   { "SendTo",               TOK_SENDTO               },
   { "Separator",            TOK_SEPARATOR            },
   { "Shade",                TOK_SHADE                },
   { "ShutdownCommand",      TOK_SHUTDOWNCOMMAND      },
   { "SnapMode",             TOK_SNAPMODE             },
   { "Spacer",               TOK_SPACER               },
   { "StartupCommand",       TOK_STARTUPCOMMAND       },
   { "Stick",                TOK_STICK                },
   { "Swallow",              TOK_SWALLOW              },
   { "TaskList",             TOK_TASKLIST             },
   { "TaskListStyle",        TOK_TASKLISTSTYLE        },
   { "Text",                 TOK_TEXT                 },
   { "Title",                TOK_TITLE                },
   { "TitleButtonOrder",     TOK_TITLEBUTTONORDER     },
   { "Tray",                 TOK_TRAY                 },
   { "TrayButton",           TOK_TRAYBUTTON           },
   { "TrayButtonStyle",      TOK_TRAYBUTTONSTYLE      },
   { "TrayStyle",            TOK_TRAYSTYLE            },
   { "Type",                 TOK_TYPE                 },
   { "Width",                TOK_WIDTH                },
   { "WindowStyle",          TOK_WINDOWSTYLE          }
};
static const unsigned int TOKEN_MAP_COUNT = ARRAY_LENGTH(TOKEN_MAP);

static TokenNode *head;

static TokenNode *CreateNode(TokenNode *current,
                             const char *file,
                             unsigned int line);
static AttributeNode *CreateAttribute(TokenNode *np); 

static char IsElementEnd(char ch);
static char IsValueEnd(char ch);
static char IsAttributeEnd(char ch);
static char *ReadElementName(const char *line);
static char *ReadValue(const char *line,
                       const char *file,
                       char (*IsEnd)(char),
                       unsigned int *offset,
                       unsigned int *lineNumber);
static char *ReadElementValue(const char *line,
                              const char *file,
                              unsigned int *offset,
                              unsigned int *lineNumber);
static char *ReadAttributeValue(const char *line,
                                const char *file,
                                unsigned int *offset,
                                unsigned int *lineNumber);
static int ParseEntity(const char *entity, char *ch,
                       const char *file, unsigned int line);
static TokenType LookupType(const char *name, TokenNode *np);

/** Tokenize data. */
TokenNode *Tokenize(const char *line, const char *fileName)
{
   TokenNode *current;
   char *temp;
   unsigned x;
   unsigned offset;
   unsigned lineNumber;
   char inElement;

   head = NULL;
   current = NULL;
   inElement = 0;
   lineNumber = 1;

   x = 0;
   /* Skip any initial white space. */
   while(IsSpace(line[x], &lineNumber)) {
      x += 1;
   }

   /* Skip any XML stuff. */
   if(!strncmp(line + x, "<?", 2)) {
      while(line[x]) {
         if(line[x] == '\n') {
            lineNumber += 1;
         }
         if(!strncmp(line + x, "?>", 2)) {
            x += 2;
            break;
         }
         x += 1;
      }
   }

   /* Process the XML data. */
   while(line[x]) {
      char found;

      /* Skip comments and white space. */
      do {

         /* Skip white space. */
         while(IsSpace(line[x], &lineNumber)) {
            x += 1;
         }

         /* Skip comments */
         found = 0;
         if(!strncmp(line + x, "<!--", 4)) {
            while(line[x]) {
               lineNumber += line[x] == '\n';
               if(!strncmp(line + x, "-->", 3)) {
                  x += 3;
                  found = 1;
                  break;
               }
               x += 1;
            }
         }

      } while(found);

      switch(line[x]) {
      case '<':
         x += 1;
         if(line[x] == '/') {

            /* Close tag. */
            x += 1;
            temp = ReadElementName(line + x);
            if(current) {
               if(JLIKELY(temp)) {
                  if(JUNLIKELY(current->type != LookupType(temp, NULL))) {
                     Warning(_("%s[%u]: close tag \"%s\" does not "
                             "match open tag \"%s\""),
                             fileName, lineNumber, temp,
                             GetTokenName(current));
                  }
               } else {
                  Warning(_("%s[%u]: unexpected and invalid close tag"),
                          fileName, lineNumber);
               }
               current = current->parent;
            } else {
               if(temp) {
                  Warning(_("%s[%u]: close tag \"%s\" without open tag"),
                          fileName, lineNumber, temp);
               } else {
                  Warning(_("%s[%u]: invalid close tag"), fileName, lineNumber);
               }
            }
            if(temp) {
               x += strlen(temp);
               Release(temp);
            }

         } else if(current && !strncmp(line + x, "![CDATA[", 8)) {

            int start, stop;

            /* CDATA */
            x += 8;
            start = x;
            while(line[x]) {
               lineNumber += line[x] == '\n';
               if(!strncmp(line + x, "]]>", 3)) {
                  x += 3;
                  break;
               }
               x += 1;
            }
            stop = x - 3;
            if(JLIKELY(stop > start)) {
               const unsigned new_len = stop - start;
               unsigned value_len = 0;
               if(current->value) {
                  value_len = strlen(current->value);
                  current->value = Reallocate(current->value,
                                              value_len + new_len + 1);
               } else {
                  current->value = Allocate(new_len + 1);
               }
               memcpy(&current->value[value_len], &line[start], new_len);
               current->value[value_len + new_len] = 0;
            }

         } else {

            /* Open tag. */
            current = CreateNode(current, fileName, lineNumber);
            temp = ReadElementName(line + x);
            if(JLIKELY(temp)) {
               x += strlen(temp);
               LookupType(temp, current);
               Release(temp);
            } else {
               Warning(_("%s[%u]: invalid open tag"), fileName, lineNumber);
            }

         }
         inElement = 1;
         break;
      case '/':

         /* End of open/close tag. */
         if(inElement) {
            x += 1;
            if(JLIKELY(line[x] == '>' && current)) {
               x += 1;
               current = current->parent;
               inElement = 0;
            } else {
               Warning(_("%s[%u]: invalid tag"), fileName, lineNumber);
            }
         } else {
            goto ReadDefault;
         }

         break;
      case '>':

         /* End of open tag. */
         x += 1;
         inElement = 0;
         break;

      default:
ReadDefault:
         if(inElement) {

            /* In the open tag; read attributes. */
            if(current) {
               AttributeNode *ap = CreateAttribute(current);
               ap->name = ReadElementName(line + x);
               if(ap->name) {
                  x += strlen(ap->name);
                  if(line[x] == '=') {
                     x += 1;
                  }
                  if(line[x] == '\"') {
                     x += 1;
                  }
                  ap->value = ReadAttributeValue(line + x, fileName,
                                                 &offset, &lineNumber);
                  x += offset;
                  if(line[x] == '\"') {
                     x += 1;
                  }
               }
            }
  
         } else {

            /* In tag body; read text. */
            temp = ReadElementValue(line + x, fileName, &offset, &lineNumber);
            x += offset;
            if(temp) {
               if(current) {
                  if(current->value) {
                     const unsigned value_len = strlen(current->value);
                     const unsigned temp_len = strlen(temp);
                     current->value = Reallocate(current->value,
                                                 value_len + temp_len + 1);
                     memcpy(&current->value[value_len], temp, temp_len + 1);
                     Release(temp);
                  } else {
                     current->value = temp;
                  }
               } else {
                  if(JUNLIKELY(temp[0])) {
                     Warning(_("%s[%u]: unexpected text: \"%s\""),
                             fileName, lineNumber, temp);
                  }
                  Release(temp);
               }
            }
         }
         break;
      }
   }

   return head;
}

/** Parse an entity reference.
 * The entity value is returned in ch and the length of the entity
 * is returned as the value of the function.
 */
int ParseEntity(const char *entity, char *ch, const char *file,
                unsigned int line)
{
   char *temp;

   if(!strncmp("&quot;", entity, 6)) {
      *ch = '\"';
      return 6;
   } else if(!strncmp("&lt;", entity, 4)) {
      *ch = '<';
      return 4;
   } else if(!strncmp("&gt;", entity, 4)) {
      *ch = '>';
      return 4;
   } else if(!strncmp("&amp;", entity, 5)) {
      *ch = '&';
      return 5;
   } else if(!strncmp("&apos;", entity, 6)) {
      *ch = '\'';
      return 6;
   } else if(!strncmp("&NewLine;", entity, 9)) {
      *ch = '\n';
      return 9;
   } else {
      unsigned int x;
      for(x = 0; entity[x]; x++) {
         if(entity[x] == ';') {
            break;
         }
      }
      if(entity[1] == '#' && entity[x] == ';') {
         if(entity[2] == 'x') {
            *ch = (char)strtol(&entity[3], NULL, 16);
         } else {
            *ch = (char)strtol(&entity[2], NULL, 10);
         }
         return x + 1;
      } else {
         temp = AllocateStack(x + 2);
         strncpy(temp, entity, x + 1);
         temp[x + 1] = 0;
         Warning(_("%s[%d]: invalid entity: \"%.8s\""), file, line, temp);
         ReleaseStack(temp);
         *ch = '&';
         return 1;
      }
   }
}

/** Determine if ch is the end of a tag/attribute name. */
char IsElementEnd(char ch)
{
   switch(ch) {
   case ' ':
   case '\t':
   case '\n':
   case '\r':
   case '\"':
   case '>':
   case '<':
   case '/':
   case '=':
   case 0:
      return 1;
   default:
      return 0;
   }
}

/** Determine if ch is the end of an attribute value. */
char IsAttributeEnd(char ch)
{
   switch(ch) {
   case 0:
   case '\"':
      return 1;
   default:
      return 0;
   }
}

/** Determine if ch is the end of tag data. */
char IsValueEnd(char ch)
{
   switch(ch) {
   case 0:
   case '<':
      return 1;
   default:
      return 0;
   }
}

/** Get the name of the next element. */
char *ReadElementName(const char *line)
{

   char *buffer;
   unsigned int len;

   /* Get the length of the element. */
   for(len = 0; !IsElementEnd(line[len]); len++);

   /* Allocate space for the element. */
   buffer = Allocate(len + 1);
   memcpy(buffer, line, len);
   buffer[len] = 0;

   return buffer;

}

/** Read the value of an element or attribute. */
char *ReadValue(const char *line,
                const char *file,
                char (*IsEnd)(char),
                unsigned int *offset,
                unsigned int *lineNumber)
{
   char *buffer;
   char ch;
   unsigned int len, max;
   unsigned int x;

   len = 0;
   max = BLOCK_SIZE;
   buffer = Allocate(max + 1);

   for(x = 0; !(IsEnd)(line[x]); x++) {
      if(line[x] == '&') {
         x += ParseEntity(line + x, &ch, file, *lineNumber) - 1;
         if(ch) {
            buffer[len] = ch;
         } else {
            buffer[len] = line[x];
         }
      } else {
         if(line[x] == '\n') {
            *lineNumber += 1;
         }
         buffer[len] = line[x];
      }
      len += 1;
      if(len >= max) {
         max += BLOCK_SIZE;
         buffer = Reallocate(buffer, max + 1);
         if(JUNLIKELY(buffer == NULL)) {
            FatalError(_("out of memory"));
         }
      }
   }
   buffer[len] = 0;
   Trim(buffer);
   *offset = x;

   return buffer;
}

/** Get the value of the current element. */
char *ReadElementValue(const char *line,
                       const char *file,
                       unsigned int *offset,
                       unsigned int *lineNumber)
{
   return ReadValue(line, file, IsValueEnd, offset, lineNumber);
}

/** Get the value of the current attribute. */
char *ReadAttributeValue(const char *line,
                         const char *file,
                         unsigned int *offset,
                         unsigned int *lineNumber)
{
   return ReadValue(line, file, IsAttributeEnd, offset, lineNumber);
}

/** Get the token for a tag name. */
TokenType LookupType(const char *name, TokenNode *np)
{
   const int x = FindValue(TOKEN_MAP, TOKEN_MAP_COUNT, name);
   if(x >= 0) {
      if(np) {
         np->type = x;
      }
      return x;
   }

   if(JUNLIKELY(np)) {
      np->type = TOK_INVALID;
      np->invalidName = CopyString(name);
   }

   return TOK_INVALID;

}

/** Get a string representation of a token. */
const char *GetTokenName(const TokenNode *tp)
{
   if(tp->invalidName) {
      return tp->invalidName;
   } else {
      return GetTokenTypeName(tp->type);
   }
}

/** Get the string representation of a token. */
const char *GetTokenTypeName(TokenType type) {
   const char *key = FindKey(TOKEN_MAP, TOKEN_MAP_COUNT, type);
   return key ? key : "[invalid]";
}

/** Create an empty XML tag node. */
TokenNode *CreateNode(TokenNode *current, const char *file,
                      unsigned int line)
{
   TokenNode *np;

   np = Allocate(sizeof(TokenNode));
   np->type = TOK_INVALID;
   np->value = NULL;
   np->attributes = NULL;
   np->subnodeHead = NULL;
   np->subnodeTail = NULL;
   np->parent = current;
   np->next = NULL;

   np->fileName = file;
   np->line = line;
   np->invalidName = NULL;

   if(current) {

      /* A node contained inside another node. */
      if(current->subnodeHead) {
         current->subnodeTail->next = np;
      } else {
         current->subnodeHead = np;
      }
      current->subnodeTail = np;

   } else if(!head) {

      /* The top-level node. */
      head = np;

   } else {

      /* A duplicate top-level node.
       * This is probably a configuration error.
       */
      ReleaseTokens(np);
      np = head->subnodeTail ? head->subnodeTail : head;

   }

   return np;
}

/** Create an empty XML attribute node. */
AttributeNode *CreateAttribute(TokenNode *np)
{
   AttributeNode *ap;
   ap = Allocate(sizeof(AttributeNode));
   ap->name = NULL;
   ap->value = NULL;
   ap->next = np->attributes;
   np->attributes = ap;
   return ap;
}

/** Release a token list. */
void ReleaseTokens(TokenNode *np)
{

   AttributeNode *ap;
   TokenNode *tp;

   while(np) {
      tp = np->next;

      while(np->attributes) {
         ap = np->attributes->next;
         if(np->attributes->name) {
            Release(np->attributes->name);
         }
         if(np->attributes->value) {
            Release(np->attributes->value);
         }
         Release(np->attributes);
         np->attributes = ap;
      }

      if(np->subnodeHead) {
         ReleaseTokens(np->subnodeHead);
      }

      if(np->value) {
         Release(np->value);
      }

      if(np->invalidName) {
         Release(np->invalidName);
      }

      Release(np);
      np = tp;
   }

}
