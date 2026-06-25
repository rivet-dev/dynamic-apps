/*
 * This file tests Apache Pig 0.12 Syntax Highlighting
 */

fake = LOAD 'fakeData' USING PigStorage() AS (name:chararray, age:int);

fake = limit fake 5; -- operations can be case insensitive

fake = FOREACH fake GENERATE TRIM(name) AS name, null as nullField, false AS booleanConst;

DUMP fake;
