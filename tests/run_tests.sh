#!/usr/bin/env bash
# End-to-end tests: feed scripts to minishell on stdin and compare output.
#
# Usage: tests/run_tests.sh [path/to/minishell]

set -u

SHELL_BIN=$(cd "$(dirname "${1:-./minishell}")" && pwd)/$(basename "${1:-./minishell}")
WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

pass=0
fail=0

# Emit a GitHub Actions error annotation; newlines must be encoded as %0A.
annotate() {
    local title=$1 msg
    shift
    msg=$(printf '%s\n' "$@" | sed -e 's/%/%25/g' | awk '{printf "%s%%0A", $0}')
    printf '::error title=FAIL %s::%s\n' "$title" "$msg"
}

# check NAME EXPECTED_STDOUT SCRIPT [EXPECTED_EXIT]
check() {
    local name=$1 expected=$2 script=$3 want_rc=${4:-0}
    local actual rc

    actual=$(cd "$WORKDIR" && printf '%s\n' "$script" | "$SHELL_BIN" 2>&1)
    rc=$?
    # Job start lines carry a PID that changes from run to run.
    actual=$(printf '%s' "$actual" | sed -E 's/^(\[[0-9]+\]) [0-9]+$/\1 PID/')

    if [[ "$actual" == "$expected" && $rc -eq $want_rc ]]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf 'FAIL: %s\n' "$name"
        printf '  script:\n%s\n' "$script" | sed 's/^/    /'
        printf '  expected (exit %s):\n%s\n' "$want_rc" "$expected" | sed 's/^/    /'
        printf '  actual (exit %s):\n%s\n' "$rc" "$actual" | sed 's/^/    /'
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            annotate "$name" "expected (exit $want_rc): $expected" "actual (exit $rc): $actual"
        fi
    fi
}

# --- tokenizer ---------------------------------------------------------------
check "plain words" "a b c" "echo a   b	c"
check "double quotes keep spaces" "a   b" 'echo "a   b"'
check "single quotes are literal" '$HOME \n' "echo '\$HOME \\n'"
check "escapes in double quotes" 'say "hi" \' 'echo "say \"hi\" \\"'
check "backslash outside quotes" "a b" 'echo a\ b'
check "adjacent pieces join" "abcdef" "echo ab\"cd\"'ef'"
check "empty quoted argument" "[]" "printf '[%s]\n' \"\""
check "comments are ignored" "kept" "echo kept # dropped"
check "unterminated quote" "minishell: syntax error: unterminated double quote" 'echo "oops' 2

# --- expansion ---------------------------------------------------------------
check "variable expansion" "v=1 [1] \$X" $'export X=1\necho v=$X "[$X]" \'$X\''
check "unset variable vanishes" "a b" 'echo a $MINISHELL_UNSET b'
check "exit status variable" $'1\n0' $'false\necho $?\necho $?'

# --- parser ------------------------------------------------------------------
check "dangling pipe" "minishell: syntax error near unexpected token \`newline'" "echo hi |" 2
check "leading pipe" "minishell: syntax error near unexpected token \`|'" "| echo hi" 2
check "missing redirect target" "minishell: syntax error near unexpected token \`newline'" "echo hi >" 2
check "ampersand not last" "minishell: syntax error near unexpected token \`echo'" "sleep 1 & echo" 2

# --- execution ---------------------------------------------------------------
check "external command" "hello" "printf 'hello\n'"
check "command not found" "minishell: no_such_cmd_xyz: command not found" "no_such_cmd_xyz" 127
check "exit status propagates" "" "false" 1
check "two-stage pipeline" "3" "printf 'a\nb\nc\n' | wc -l | tr -d ' '"
check "multi-stage pipeline" $'2 b\n1 a' "printf 'b\na\nb\n' | sort | uniq -c | sort -rn | sed 's/^ *//'"
check "pipeline status is last stage" "" "false | true"
check "early-exiting reader" "y" "yes | head -1"
check "output redirection" "one" $'echo one > out1\ncat out1'
check "append redirection" $'one\ntwo' $'echo one > out2\necho two >> out2\ncat out2'
check "input redirection" "2" $'printf "x\\ny\\n" > in1\nwc -l < in1 | tr -d " "'
check "redirections with pipeline" "B" $'echo b > in2\ncat < in2 | tr a-z A-Z > out3\ncat out3'
check "missing input file" "minishell: nofile: No such file or directory" "cat < nofile" 1

# --- background jobs ---------------------------------------------------------
check "background job is reaped" $'[1] PID\n[1]  Done\t\ttrue &' $'true &\nsleep 0.3' 0
check "background job exit status" $'[1] PID\n[1]  Exit 3\t\tsh -c \'exit 3\' &' $'sh -c \'exit 3\' &\nsleep 0.3' 0
check "foreground runs while job is busy" $'[1] PID\nfg\n[1]  Done\t\tsleep 0.2 &' $'sleep 0.2 &\necho fg\nsleep 1' 0
# strsignal() text differs between libcs ("Terminated" vs "Terminated: 15").
if [[ "$(uname)" == Darwin ]]; then term_msg="Terminated: 15"; else term_msg="Terminated"; fi
check "killed foreground command" "$term_msg" 'sh -c "kill -TERM \$\$"' 143

# --- built-ins ---------------------------------------------------------------
check "pwd" "$(cd "$WORKDIR" && pwd -P)" "pwd"
check "cd changes directory" "/" $'cd /\npwd'
check "cd with no args goes home" "$HOME" $'cd\npwd'
check "cd - returns" "$(cd "$WORKDIR" && pwd -P)" $'cd /\ncd -'
check "cd to missing dir" "minishell: cd: /no/such/dir: No such file or directory" "cd /no/such/dir" 1
check "export reaches children" "bar" $'export FOO=bar\nsh -c \'echo $FOO\''
check "export rejects bad names" "minishell: export: \`1x=2': not a valid identifier" "export 1x=2" 1
check "builtin output redirection" "/" $'cd /\npwd > '"$WORKDIR"$'/pwd.txt\ncat '"$WORKDIR"'/pwd.txt'
check "history lists commands" $'a\n    1  echo a\n    2  history' $'echo a\nhistory' 0
check "history N" $'a\nb\n    2  echo b\n    3  history 2' $'echo a\necho b\nhistory 2' 0
check "exit with code" "" $'exit 42\necho unreachable' 42
check "exit uses last status" "" $'false\nexit' 1
check "exit in pipeline does not exit shell" "still here" $'exit 3 | true\necho still here'
check "builtin in pipeline" "/" $'cd /\npwd | cat'

printf '\n%d passed, %d failed\n' "$pass" "$fail"
[[ $fail -eq 0 ]]
