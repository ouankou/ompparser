(() => {
  const LangMode = {
    auto: 0,
    c: 1,
    cpp: 2,
    fortran: 3
  };

  const inputTextArea = document.getElementById("input-editor");
  const outputTextArea = document.getElementById("output-editor");
  const parseButton = document.getElementById("parse-btn");
  const fileInput = document.getElementById("file-input");
  const langSelect = document.getElementById("lang-select");
  const statusPill = document.getElementById("wasm-status");
  const logOutput = document.getElementById("log-output");
  const tabButtons = document.querySelectorAll(".tab");
  const tabPanels = document.querySelectorAll(".tab-panel");

  const logState = {
    lines: []
  };

  let wasmModule = null;
  let lastFileName = "";

  const inputEditor = CodeMirror.fromTextArea(inputTextArea, {
    lineNumbers: true,
    theme: "molokai",
    mode: "text/x-csrc",
    tabSize: 2,
    indentUnit: 2,
    viewportMargin: Infinity
  });

  const outputEditor = CodeMirror.fromTextArea(outputTextArea, {
    lineNumbers: true,
    theme: "molokai",
    mode: "text/x-csrc",
    readOnly: true,
    tabSize: 2,
    indentUnit: 2,
    viewportMargin: Infinity
  });

  const SAMPLE = "#pragma omp parallel for\nfor (int i = 0; i < n; ++i) {\n  work(i);\n}\n";
  inputEditor.setValue(SAMPLE);

  function setActiveTab(name) {
    tabButtons.forEach((tab) => {
      tab.classList.toggle("is-active", tab.dataset.tab === name);
    });
    tabPanels.forEach((panel) => {
      panel.classList.toggle("is-active", panel.dataset.panel === name);
    });
    if (name === "unparse")
      outputEditor.refresh();
  }

  function appendLog(text) {
    if (typeof text !== "string" || !text.length)
      return;
    logState.lines.push(text);
    logOutput.textContent = logState.lines.join("\n");
  }

  function clearLog() {
    logState.lines = [];
    logOutput.textContent = "";
  }

  function detectLangHintFromFileName(name) {
    const lower = (name || "").toLowerCase();
    if (lower.endsWith(".f") || lower.endsWith(".for") ||
        lower.endsWith(".f90") || lower.endsWith(".f95") ||
        lower.endsWith(".f03") || lower.endsWith(".f08") ||
        lower.endsWith(".f77") || lower.endsWith(".f66") ||
        lower.endsWith(".fpp") || lower.endsWith(".f18") ||
        lower.endsWith(".f23")) {
      return LangMode.fortran;
    }
    if (lower.endsWith(".cc") || lower.endsWith(".cpp") ||
        lower.endsWith(".cxx") || lower.endsWith(".c++")) {
      return LangMode.cpp;
    }
    if (lower.endsWith(".c")) {
      return LangMode.c;
    }
    return null;
  }

  function detectLangHintFromContent(text) {
    if (/^[\s]*[!cC*]\$omp/im.test(text))
      return LangMode.fortran;
    return LangMode.c;
  }

  function resolveLanguageSelection() {
    const selected = langSelect.value;
    if (selected === "c")
      return { mode: LangMode.c, hint: LangMode.c, editorMode: "text/x-csrc" };
    if (selected === "cpp")
      return {
        mode: LangMode.cpp,
        hint: LangMode.cpp,
        editorMode: "text/x-c++src"
      };
    if (selected === "fortran")
      return {
        mode: LangMode.fortran,
        hint: LangMode.fortran,
        editorMode: "text/x-fortran"
      };

    const hintFromFile = detectLangHintFromFileName(lastFileName);
    const hint = hintFromFile || detectLangHintFromContent(inputEditor.getValue());
    const editorMode = hint === LangMode.fortran
      ? "text/x-fortran"
      : (hint === LangMode.cpp ? "text/x-c++src" : "text/x-csrc");
    return { mode: LangMode.auto, hint, editorMode };
  }

  function updateEditorModes() {
    const { editorMode } = resolveLanguageSelection();
    inputEditor.setOption("mode", editorMode);
    outputEditor.setOption("mode", editorMode);
  }

  tabButtons.forEach((tab) => {
    tab.addEventListener("click", () => setActiveTab(tab.dataset.tab));
  });

  fileInput.addEventListener("change", (event) => {
    const file = event.target.files && event.target.files[0];
    if (!file)
      return;
    const reader = new FileReader();
    reader.onload = () => {
      const content = typeof reader.result === "string" ? reader.result : "";
      inputEditor.setValue(content);
      lastFileName = file.name;
      updateEditorModes();
    };
    reader.readAsText(file);
  });

  langSelect.addEventListener("change", updateEditorModes);

  parseButton.addEventListener("click", () => {
    if (!wasmModule)
      return;

    clearLog();
    setActiveTab("unparse");

    const inputText = inputEditor.getValue();
    const { mode, hint } = resolveLanguageSelection();
    const output = wasmModule.parseAndUnparse(inputText, mode, hint);

    outputEditor.setValue(output || "");

    const hasError = logState.lines.some((line) => line.includes("error:"));
    if (hasError)
      setActiveTab("log");
  });

  if (typeof createOmpparserModule !== "function") {
    appendLog("error: ompparser module not loaded.");
    setActiveTab("log");
    return;
  }

  createOmpparserModule({
    print: (text) => appendLog(text),
    printErr: (text) => appendLog(text)
  })
    .then((moduleInstance) => {
      wasmModule = moduleInstance;
      statusPill.textContent = "wasm ready";
      statusPill.classList.add("is-ready");
      parseButton.disabled = false;
    })
    .catch((err) => {
      appendLog(`error: ${err && err.message ? err.message : err}`);
      setActiveTab("log");
    });
})();
