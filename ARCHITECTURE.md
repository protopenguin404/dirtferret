# ARCHITECTURE Overview

** General Layout **
This Program will borrow heavily from the nvim architecture. namely, that is to say that each 'browser'
(tab) is equivalent to one 'buffer.' To accomplish this, we are conceptually going to separate each 
layer of responsibility. There are four, as of my current mental model:

1. ** CEF Backend **
    This will be, primarily, just a dumb api, as much as we can reasonably relegate it as such. we 
    effectively just want to expose its own API in such a way that it can be called into. There is an
    important clarification to be made in section 4. this will ideally run as a daemon
    addendum: we will probably implement all of the lifecycle, buffer management, history and so forth
    here too; though that could optionally be an additional layer or folded into sect 4

2. ** Kitty Based Frontend **
    This, similar to the backend, is a dumb pixel renderer and input capturing layer. This will be the 
    bit where we can define any keyboard/layout stuff, exposed so that plugins do the control. 

3. ** Plugin/Lua runtime **
    This one of two of the most important bits, and where we diverge from nvim some. Importantly, any
    and all policy logic will live here. This layer has one sole task of coordinating and interpereting
    all policy (policy, as in, user defined behavior). As a default, we will include our own, small 
    suite of overridable/editable plugins. This is to enable maximum configurability for the end user,
    without requiring them to create a massive config for basic functionality. If possible, the typical
    browser concept of extensions should live here too, or in an adjacent similar sister layer. We will
    embed Lua as the primary plugin engine, but at some point I would like to allow for both IPC Based
    plugins, as well as C++ ABI/FFI's. The latter will be primarily community maintained/created but I
    personally wouldn't mind making one, for arguments sake, Haskell (FP text search ftw)

4. ** API Central Routing **
    The second of the two most important bits. The clarification I wanted to make was this: The other
    three layers will expose APIs, but not to the end user. They expose them only to this layer. This
    layer will be where we compile and unify all singular "actions" to be exposed via the user facing
    configuration API; as well as routing individual commands and queries to and from the relevant 
    caller/recipients for internal stuff. That is to say, this layer will essentially house all of the
    "mechanical" logic (as opposed to 'policy' logic) that the program needs to actually coordinate 
    and execute all of the timing sensitive interactions. The API should ideally be granular, and 
    include two categories of calls, being "commands" and "queries." Commands should always be fire
    and forget with simple metadata type args, while queries specifically do NOT actually modify state,
    only return information. They both should be first class citizens, and by that I mean, internally 
    at least, you can sort of treat them like whole objects with factories so they can be encapsulated
    into other behaviors. This doesnt need to be 100% literal, but in effect its more than just "send 
    some text and off you go."

## Diagram 
                                CEF Backend
                                    |
                                    |
                                    |
            Frontend ----------- API Layer ------------ Plugin/Lua runtime

## Terminology

Buffer      - This is a CEF instance. one single lifetime, and any metadata relevant to it.

Policy      - Any logic that could be relegated to user preference. If you need to ask the user something,
              it is policy logic.

Mechanics   - Any logic that is entirely internal. Stuff that the user probably would never want to config
              themselves.

Command     - A single action that must cross boundaries, and optional metadata.

Query       - A single non-side-effectual query of state/environment.

Internal    - (With regard to API) API interaction that is NOT user facing.

External    - (With regard to API) API interaction that is strictly user defined, expicitly not exposed to
              internal API.

## Summary

So, you may notice, all 4 should be, essentially, completely independant processes that (technically) 
do not need the others to actually be 'running,' but, as a whole, they complete the vision. Total and
absolute separation of concerns with an iron fist will be the most important design philosophy here; 
and these splits will be the first thing that gets implemented, at least at a basic level so that 
future additions have very exact locations to be added, and methods to interact. Additionally, we will
borrow philosophically from Nix, as a concept, and focus on declarativity, explicivity, and reproducability.
We also should dedicate equal focus to clear, unambiguous, and beginner friendly logging and documentation.
(though, not to go as far as being pedantic, I doubt true noobs would find/want this browser. This is,
clearly, a browser targetsd at devs/terminal power users)

This document may change slightly as I learn and grow, but should remain the general founding doctrine/vision
statement for the browser at large. Claude should ask questions when needed, and take notes in his own
CLAUDE.md.

