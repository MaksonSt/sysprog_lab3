import { useEffect, useRef } from "react";

export default function SourceView({ name, source, activeLine, onLineClick }) {
    const listRef = useRef(null);
    const lines = (source || "вихідний код відсутній у файлі").replace(/\n$/, "").split("\n");

    useEffect(() => {
        const row = listRef.current?.querySelector(".line.active");
        row?.scrollIntoView({ block: "nearest" });
    }, [activeLine]);

    return (
        <section className="panel source-panel">
            <div className="panel-head">
                <h2>Код C++{name && <span className="muted"> · {name}</span>}</h2>
            </div>
            <div className="code-view" ref={listRef}>
                {lines.map((text, i) => (
                    <div
                        key={i}
                        className={"line" + (activeLine === i + 1 ? " active" : "")}
                        onClick={() => onLineClick(i + 1)}
                    >
                        <span className="line-number">{i + 1}</span>
                        <span>{text || " "}</span>
                    </div>
                ))}
            </div>
        </section>
    );
}
