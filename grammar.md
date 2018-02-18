# PBRT format's Grammar

In this document the grammar recognized by the PBRTParser implementation is presented. 
In particular, file format specifications described in the [pbrt page](http://www.pbrt.org/fileformat-v3.html).

Characters enclosed in between two `*` symbols represents terminal symbols in the grammar.

```
S                     --->    StartDirectives **WorldBegin** *\n* WB *WorldEnd*
StartDirectives       --->    Statement | Statement StartDirectives
Statement             --->    Identifier Type ParameterList
Identifier            --->    [alfa][alfanum]+
Type                  --->    *integer* | *float* | *point2* | *vector2* | *point3* | *vector3*
                              *normal3* | *spectrum* | *bool* | *string* | *point* | *normal*
Name                  --->    Identifier
ParameterList         --->    e |  NameValuePair ParameterList
NameValuePair         --->    *"* Type Name *"* ValueList 
ValueList             --->    Value | *[* Values *]*
Values                --->    Value * * Values | Value
Value                 --->    *integer* | *float* |




Comment               --->    *#* [zero or more symbols (any symbol)] *\n*
```