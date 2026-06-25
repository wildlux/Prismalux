"""Classificazione capacità modelli LLM — equivalente Python di PrismaluxPaths::ModelCap."""

CAP_NONE      = 0x00
CAP_EMBEDDING = 0x01
CAP_VISION    = 0x02
CAP_TOOLS     = 0x04
CAP_THINKING  = 0x08

DEFAULT_EMBED_MODEL = "embeddinggemma"

_EMBEDDING = {
    "embeddinggemma", "nomic-embed-text", "mxbai-embed-large",
    "all-minilm", "snowflake-arctic-embed", "bge-m3",
    "bge-large", "nomic-embed", "mxbai-embed", "snowflake-arctic",
    "paraphrase-multilingual",
}
_VISION = {
    "llava", "bakllava", "moondream", "cogvlm", "minicpm-v",
    "llama3.2-vision", "llama4", "gemma3", "qwen-vl", "internvl",
    "minicpm", "phi-3-vision", "pixtral",
}
_TOOLS = {
    "mistral", "mixtral", "llama3", "llama3.1", "llama3.2",
    "llama3.3", "qwen2.5", "qwen3", "firefunction",
    "command-r", "hermes", "nous-hermes", "openhermes",
    "functionary", "gorilla", "llama4",
}
_THINKING = {
    "qwen3", "qwq", "deepseek-r1", "qwen2.5", "marco-o1",
    "sky-t1", "llama3.3",
}


def model_capabilities(name: str) -> int:
    n = name.lower().split(":")[0]
    caps = CAP_NONE
    for kw in _EMBEDDING:
        if kw in n:
            caps |= CAP_EMBEDDING
    for kw in _VISION:
        if kw in n:
            caps |= CAP_VISION
    for kw in _TOOLS:
        if kw in n:
            caps |= CAP_TOOLS
    for kw in _THINKING:
        if kw in n:
            caps |= CAP_THINKING
    return caps


def model_cap_badge(caps: int) -> str:
    parts: list[str] = []
    if caps & CAP_VISION:    parts.append("👁")
    if caps & CAP_TOOLS:     parts.append("🔧")
    if caps & CAP_THINKING:  parts.append("🧠")
    if caps & CAP_EMBEDDING: parts.append("[emb]")
    return " ".join(parts)


def labeled(name: str) -> str:
    """Restituisce 'nomeModello  👁 🔧' pronto per il dropdown."""
    badge = model_cap_badge(model_capabilities(name))
    return f"{name}  {badge}" if badge else name


def is_embedding_model(name: str) -> bool:
    return bool(model_capabilities(name) & CAP_EMBEDDING)

def is_vision_model(name: str) -> bool:
    return bool(model_capabilities(name) & CAP_VISION)

def is_tools_model(name: str) -> bool:
    return bool(model_capabilities(name) & CAP_TOOLS)

def is_thinking_model(name: str) -> bool:
    return bool(model_capabilities(name) & CAP_THINKING)
