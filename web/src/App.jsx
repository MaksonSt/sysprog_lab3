import { useState } from "react";
import examples from "./examples.json";
import AstTree from "./components/AstTree.jsx";
import JsPanel from "./components/JsPanel.jsx";
import SourceView from "./components/SourceView.jsx";

function parseFile(name, text) {
    const data = JSON.parse(text);
    if (data.ast) return data;
    if (data.kind) return { name, ast: data };
    throw new Error("це не AST і не пакет minicpp");
}

export default function App() {
    const [bundle, setBundle] = useState(examples[0] || null);
    const [selected, setSelected] = useState(examples.length ? "0" : "");
    const [activeLine, setActiveLine] = useState(null);
    const [version, setVersion] = useState(0);
    const [dragging, setDragging] = useState(false);

    function open(next, key) {
        setBundle(next);
        setSelected(key);
        setActiveLine(null);
        setVersion((v) => v + 1);
    }

    async function openFile(file) {
        if (!file) return;
        try {
            open(parseFile(file.name, await file.text()), "");
        } catch (error) {
            alert(`Не вдалося відкрити ${file.name}: ${error.message}`);
        }
    }

    function onDrop(event) {
        event.preventDefault();
        setDragging(false);
        openFile(event.dataTransfer.files[0]);
    }

    return (
        <div
            className="app"
            onDragOver={(e) => { e.preventDefault(); setDragging(true); }}
            onDragLeave={(e) => { if (!e.currentTarget.contains(e.relatedTarget)) setDragging(false); }}
            onDrop={onDrop}
        >
            <header className="topbar">
                <div className="brand">
                    <span className="brand-name">minicpp</span>
                    <span className="muted">дерево розбору C++</span>
                </div>
                <div className="actions">
                    <label className="field">
                        <span>Приклад</span>
                        <select value={selected} onChange={(e) => open(examples[Number(e.target.value)], e.target.value)}>
                            {examples.map((ex, i) => (
                                <option key={ex.name} value={String(i)}>{ex.name}</option>
                            ))}
                        </select>
                    </label>
                </div>
            </header>

            {bundle ? (
                <main className="layout">
                    <SourceView
                        name={bundle.name}
                        source={bundle.source}
                        activeLine={activeLine}
                        onLineClick={setActiveLine}
                    />
                    <AstTree
                        ast={bundle.ast}
                        activeLine={activeLine}
                        onNodeClick={(node) => node.line && setActiveLine(node.line)}
                    />
                    <JsPanel key={version} js={bundle.js} defaultInput={bundle.stdin} />
                </main>
            ) : (
                <div className="empty">
                    Немає вбудованих прикладів. Запустіть <code>build.bat web</code> або відкрийте JSON.
                </div>
            )}

            {dragging && <div className="drop-hint">Відпустіть файл .json, щоб відкрити</div>}
        </div>
    );
}
