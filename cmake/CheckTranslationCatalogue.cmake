# Compares a translation catalogue against an inventory of the source strings
# lupdate currently extracts, and fails when the catalogue is behind. The
# inventory is a throwaway catalogue regenerated in the build tree, so the
# comparison is over source strings rather than file text: lupdate's ordering
# and formatting are not a contract, and the tracked catalogue is
# hand-maintained where lupdate cannot reach.

if(NOT DEFINED EDIT_ATLAS_TRANSLATION_CATALOGUE)
    message(FATAL_ERROR "EDIT_ATLAS_TRANSLATION_CATALOGUE is required.")
endif()

if(NOT DEFINED EDIT_ATLAS_TRANSLATION_INVENTORY)
    message(FATAL_ERROR "EDIT_ATLAS_TRANSLATION_INVENTORY is required.")
endif()

foreach(
    edit_atlas_translation_file
    IN
    ITEMS
        "${EDIT_ATLAS_TRANSLATION_CATALOGUE}"
        "${EDIT_ATLAS_TRANSLATION_INVENTORY}"
)
    if(NOT EXISTS "${edit_atlas_translation_file}")
        message(
            FATAL_ERROR
            "Translation file does not exist: ${edit_atlas_translation_file}"
        )
    endif()
endforeach()

# Source strings carry arbitrary punctuation, semicolons among it, so entries
# are accumulated into delimited strings and compared with string(FIND)
# instead of being held in CMake lists.
string(ASCII 30 edit_atlas_entry_separator)
string(ASCII 31 edit_atlas_field_separator)

# Turns escaped catalogue text back into what a reader sees in the interface.
function(edit_atlas_readable_text value output)
    string(REPLACE "&lt;" "<" value "${value}")
    string(REPLACE "&gt;" ">" value "${value}")
    string(REPLACE "&quot;" "\"" value "${value}")
    string(REPLACE "&apos;" "'" value "${value}")
    string(REPLACE "&amp;" "&" value "${value}")
    set("${output}" "${value}" PARENT_SCOPE)
endfunction()

# Splits a delimited set of entries into a list, one element per entry, with
# each entry's own semicolons escaped so it survives as a single element. The
# escaping only holds while the list is iterated: a list() command rewrites
# the value and drops it.
function(edit_atlas_entry_list entries output)
    string(REPLACE ";" "\\;" entries "${entries}")
    string(
        REPLACE
        "${edit_atlas_entry_separator}"
        ";"
        entries
        "${entries}"
    )
    set("${output}" "${entries}" PARENT_SCOPE)
endfunction()

# Collects every message in a catalogue as a context and source pair, the
# subset whose translation is missing or still marked unfinished, and the
# contexts that carry at least one message.
function(
    edit_atlas_read_catalogue
    path
    entries_output
    unfinished_output
    contexts_output
)
    file(READ "${path}" content)

    set(entries "")
    set(unfinished "")
    set(contexts "")
    set(context "")

    while(TRUE)
        string(FIND "${content}" "<name>" name_start)
        string(FIND "${content}" "<message>" message_start)
        if(name_start EQUAL -1 AND message_start EQUAL -1)
            break()
        endif()

        if(
            NOT name_start EQUAL -1
            AND (message_start EQUAL -1 OR name_start LESS message_start)
        )
            string(SUBSTRING "${content}" ${name_start} -1 content)
            if(NOT content MATCHES "^<name>([^<]*)</name>")
                message(FATAL_ERROR "Malformed context name in ${path}.")
            endif()
            set(context "${CMAKE_MATCH_1}")
            string(LENGTH "<name>${context}</name>" consumed)
            string(SUBSTRING "${content}" ${consumed} -1 content)
            continue()
        endif()

        string(SUBSTRING "${content}" ${message_start} -1 content)
        string(FIND "${content}" "</message>" message_end)
        if(message_end EQUAL -1)
            message(FATAL_ERROR "Unterminated message in ${path}.")
        endif()
        string(SUBSTRING "${content}" 0 ${message_end} block)
        string(SUBSTRING "${content}" ${message_end} -1 content)

        if(NOT block MATCHES "<source>(.*)</source>")
            message(FATAL_ERROR "Message without a source string in ${path}.")
        endif()
        # Compared unescaped, because lupdate and a hand-edited catalogue
        # escape the same apostrophe differently.
        edit_atlas_readable_text("${CMAKE_MATCH_1}" source)
        set(entry
            "${edit_atlas_entry_separator}${context}${edit_atlas_field_separator}${source}"
        )
        string(APPEND entries "${entry}")
        set(delimited_context
            "${edit_atlas_entry_separator}${context}${edit_atlas_entry_separator}"
        )
        string(FIND "${contexts}" "${delimited_context}" known)
        if(known EQUAL -1)
            string(APPEND contexts "${delimited_context}")
        endif()

        # A plural translation holds its forms in child elements, so the
        # elements are stripped and what remains decides whether anything was
        # translated at all.
        if(block MATCHES "<translation([^>]*)>(.*)</translation>")
            set(attributes "${CMAKE_MATCH_1}")
            set(translation "${CMAKE_MATCH_2}")
        elseif(block MATCHES "<translation([^>]*)/>")
            set(attributes "${CMAKE_MATCH_1}")
            set(translation "")
        else()
            set(attributes "")
            set(translation "")
        endif()
        string(REGEX REPLACE "<[^>]*>" "" translation "${translation}")
        string(STRIP "${translation}" translation)
        if(attributes MATCHES "unfinished" OR translation STREQUAL "")
            string(APPEND unfinished "${entry}")
        endif()
    endwhile()

    if(entries)
        string(APPEND entries "${edit_atlas_entry_separator}")
    endif()
    if(unfinished)
        string(APPEND unfinished "${edit_atlas_entry_separator}")
    endif()
    set("${entries_output}" "${entries}" PARENT_SCOPE)
    set("${unfinished_output}" "${unfinished}" PARENT_SCOPE)
    set("${contexts_output}" "${contexts}" PARENT_SCOPE)
endfunction()

# Reports the entries of one delimited set that the other does not contain.
function(edit_atlas_missing_entries entries known output)
    set(missing "")
    edit_atlas_entry_list("${entries}" candidates)
    foreach(candidate IN LISTS candidates)
        if(candidate STREQUAL "")
            continue()
        endif()
        set(delimited
            "${edit_atlas_entry_separator}${candidate}${edit_atlas_entry_separator}"
        )
        string(FIND "${known}" "${delimited}" found)
        if(found EQUAL -1)
            string(APPEND missing "${delimited}")
        endif()
    endforeach()
    set("${output}" "${missing}" PARENT_SCOPE)
endfunction()

# Renders entries as context and source lines a translator can act on.
function(edit_atlas_describe_entries entries output)
    set(description "")
    edit_atlas_entry_list("${entries}" listed)
    foreach(entry IN LISTS listed)
        if(entry STREQUAL "")
            continue()
        endif()
        string(FIND "${entry}" "${edit_atlas_field_separator}" boundary)
        string(SUBSTRING "${entry}" 0 ${boundary} context)
        math(EXPR boundary "${boundary} + 1")
        string(SUBSTRING "${entry}" ${boundary} -1 source)
        string(APPEND description "  ${context}: ${source}\n")
    endforeach()
    set("${output}" "${description}" PARENT_SCOPE)
endfunction()

edit_atlas_read_catalogue(
    "${EDIT_ATLAS_TRANSLATION_CATALOGUE}"
    edit_atlas_catalogue_entries
    edit_atlas_catalogue_unfinished
    edit_atlas_catalogue_contexts
)
edit_atlas_read_catalogue(
    "${EDIT_ATLAS_TRANSLATION_INVENTORY}"
    edit_atlas_inventory_entries
    edit_atlas_inventory_unfinished
    edit_atlas_inventory_contexts
)

if(NOT edit_atlas_inventory_entries)
    message(
        FATAL_ERROR
        "No source strings were extracted. The inventory at "
        "${EDIT_ATLAS_TRANSLATION_INVENTORY} is empty, which means lupdate "
        "reached none of the sources rather than that the catalogue is "
        "complete."
    )
endif()

edit_atlas_missing_entries(
    "${edit_atlas_inventory_entries}"
    "${edit_atlas_catalogue_entries}"
    edit_atlas_untranslated
)
edit_atlas_missing_entries(
    "${edit_atlas_catalogue_entries}"
    "${edit_atlas_inventory_entries}"
    edit_atlas_unreachable
)
edit_atlas_missing_entries(
    "${edit_atlas_catalogue_contexts}"
    "${edit_atlas_inventory_contexts}"
    edit_atlas_absent_contexts
)

get_filename_component(
    edit_atlas_catalogue_name
    "${EDIT_ATLAS_TRANSLATION_CATALOGUE}"
    NAME
)

# An entry lupdate no longer extracts is reported without failing. It costs
# nothing at runtime, it can be one the catalogue carries deliberately, and
# revising existing translations is a separate decision.
if(edit_atlas_unreachable)
    edit_atlas_describe_entries(
        "${edit_atlas_unreachable}"
        edit_atlas_unreachable_description
    )
    message(
        STATUS
        "${edit_atlas_catalogue_name} translates strings the sources no "
        "longer contain:\n${edit_atlas_unreachable_description}"
    )
endif()

set(edit_atlas_report "")

# A context that lost every one of its strings is not stale translation: the
# extractor stopped reading a file it used to read. lupdate silently ignores
# QML unless qttools was built with its QML parser, which is how the Qt Quick
# interface fell out of coverage before, so this fails rather than warns.
if(edit_atlas_absent_contexts)
    edit_atlas_entry_list(
        "${edit_atlas_absent_contexts}"
        edit_atlas_listed_contexts
    )
    string(APPEND edit_atlas_report "Contexts no source string reached:\n")
    foreach(edit_atlas_context IN LISTS edit_atlas_listed_contexts)
        if(edit_atlas_context STREQUAL "")
            continue()
        endif()
        string(APPEND edit_atlas_report "  ${edit_atlas_context}\n")
    endforeach()
    string(
        APPEND
        edit_atlas_report
        "Their file was renamed or removed, or lupdate could not read it: it "
        "skips QML unless qttools was built with its QML parser.\n"
    )
endif()
if(edit_atlas_untranslated)
    edit_atlas_describe_entries(
        "${edit_atlas_untranslated}"
        edit_atlas_untranslated_description
    )
    string(
        APPEND
        edit_atlas_report
        "Source strings missing from the catalogue:\n"
        "${edit_atlas_untranslated_description}"
    )
endif()
if(edit_atlas_catalogue_unfinished)
    edit_atlas_describe_entries(
        "${edit_atlas_catalogue_unfinished}"
        edit_atlas_unfinished_description
    )
    string(
        APPEND
        edit_atlas_report
        "Catalogue entries left untranslated:\n"
        "${edit_atlas_unfinished_description}"
    )
endif()

if(edit_atlas_report)
    message(
        FATAL_ERROR
        "${edit_atlas_catalogue_name} does not cover the interface.\n"
        "${edit_atlas_report}"
        "See the translations section of CONTRIBUTING.md."
    )
endif()

message(
    STATUS
    "Verified ${edit_atlas_catalogue_name} against the extracted source "
    "strings."
)
