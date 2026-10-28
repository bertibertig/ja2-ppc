# Sourced by the scripts in tools/. Loads the machine-specific paths from the
# git-ignored local.env in the repository root (see local.env.example).
# Variables already set in the environment take precedence.

load_local_env() {
    local file=$1 key _
    [[ -f "$file" ]] || return 0
    while IFS='=' read -r key _; do
        [[ "$key" =~ ^[A-Z_][A-Z0-9_]*$ && -z "${!key:-}" ]] || continue
        export "$key=$(set -a; . "$file"; printf '%s' "${!key}")"
    done < "$file"
}

load_local_env "$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)/local.env"
