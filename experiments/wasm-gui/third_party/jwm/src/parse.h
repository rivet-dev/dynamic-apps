/**
 * @file parse.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header file for the JWM configuration parser.
 *
 */

#ifndef PARSE_H
#define PARSE_H

struct Menu;

/** Parse a configuration file.
 * @param fileName The user-specified config file to parse.
 */
void ParseConfig(const char *fileName);

/** Parse a dynamic menu.
 * @param timeout_ms The timeout in milliseconds.
 * @param command The command to generate the menu.
 * @return The menu.
 */
struct Menu *ParseDynamicMenu(unsigned timeout_ms, const char *command);

#endif /* PARSE_H */

