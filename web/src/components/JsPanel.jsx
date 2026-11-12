import { useEffect, useRef, useState } from "react";
import { runJs, TIMEOUT_SECONDS } from "../runner.js";

const RUNTIME_END = "})();\n";

function splitRuntime(js) {
    const end = js.indexOf(RUNTIME_END);
    return end < 0 ? js : js.slice(end + RUNTIME_END.length).replace(/^\n+/, "");
}

export default function JsPanel({ js, defaultInput }) {
    const [showRuntime, setShowRuntime] = useState(false);
    const [stdin, setStdin] = useState(defaultInput || "");
    const [output, setOutput] = useState([]);
    const [running, setRunning] = useState(false);
    const [copied, setCopied] = useState(false);
    const stopRef = useRef(null);
    const outputRef = useRef(null);

    useEffect(() => () => stopRef.current?.(), []);
    useEffect(() => {
        if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }, [output]);

    const append = (text, kind) => setOutput((parts) => [...parts, { text, kind }]);

    function run() {
        stopRef.current?.();
        setOutput([]);
        setRunning(true);
        stopRef.current = runJs(js, stdin, {
            onOutput: (text, isError) => append(text, isError ? "error" : null),
            onDone: ({ exitCode, timedOut, ms }) => {
                if (timedOut) append(`\n[зупинено: виконання триває довше ${TIMEOUT_SECONDS} с]\n`, "error");
                else append(`\n[завершено з кодом ${exitCode} за ${ms} мс]\n`, "status");
                stopRef.current = null;
                setRunning(false);
            },
        });
    }

    async function copy() {
        await navigator.clipboard.writeText(js);
        setCopied(true);
        setTimeout(() => setCopied(false), 1200);
    }

    if (!js) {
        return (
            <section className="panel js-panel">
                <div className="panel-head"><h2>JavaScript</h2></div>
                <div className="empty">JavaScript відсутній у файлі (відкрито лише AST)</div>
            </section>
        );
    }

    return (
        <section className="panel js-panel">
            <div className="panel-head">
                <h2>JavaScript</h2>
                <div className="actions">
                    <button className="button small" onClick={copy}>{copied ? "Скопійовано" : "Копіювати"}</button>
                    <button className="button" onClick={run} disabled={running}>
                        {running ? "Виконується…" : "Запустити"}
                    </button>
                </div>
            </div>

            <pre className="code-view js-view">{showRuntime ? js : splitRuntime(js)}</pre>

            <div className="io">
                <label className="io-block">
                    <span>Вхідні дані (cin)</span>
                    <textarea
                        rows={2}
                        spellCheck={false}
                        placeholder="значення через пробіл або з нового рядка"
                        value={stdin}
                        onChange={(e) => setStdin(e.target.value)}
                    />
                </label>
                <div className="io-block">
                    <span>Вивід</span>
                    <pre className="output" ref={outputRef}>
                        {output.map((part, i) => (
                            <span key={i} className={part.kind || undefined}>{part.text}</span>
                        ))}
                    </pre>
                </div>
            </div>
        </section>
    );
}
