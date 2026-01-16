import { useState, useEffect } from 'react';
import './App.css';
import { EventsOn } from '../wailsjs/runtime';
import * as Processor from '../wailsjs/go/main/ImageProcessor';

function App() {
    const [jobs, setJobs] = useState([]);

    useEffect(() => {
        // Load existing jobs of jobs
        Processor.GetJobs().then(data => {
            if (data) setJobs(data);
        }).catch(err => console.error("Error loading jobs:", err));

        // Listen for progress updates from Go
        const unsubscribeUpdate = EventsOn("jobUpdate", (updatedJob) => {
            setJobs(prev => {
                const index = prev.findIndex(j => j.id === updatedJob.id);
                if (index >= 0) {
                    const next = [...prev];
                    next[index] = updatedJob;
                    return next;
                }
                return [updatedJob, ...prev];
            });
        });

        // Handle incoming file drops from Wails
        const unsubscribeDrop = EventsOn("wails:file-drop", (x, y, paths) => {
            console.log("Wails: Drop received", paths);
            if (paths && paths.length > 0) {
                paths.forEach(path => {
                    Processor.ProcessFolder(path).catch(console.error);
                });
            }
        });

        // PREVENT BROWSER NAVIGATION
        const preventDefault = (e) => {
            e.preventDefault();
            e.stopPropagation();
        };

        window.addEventListener('dragover', preventDefault, false);
        window.addEventListener('drop', preventDefault, false);

        return () => {
            window.removeEventListener('dragover', preventDefault);
            window.removeEventListener('drop', preventDefault);
            unsubscribeUpdate();
            unsubscribeDrop();
        };
    }, []);

    const handlePause = (id) => Processor.PauseJob(id);
    const handleResume = (id) => Processor.ResumeJob(id);
    const handleStop = (id) => Processor.StopJob(id);
    const handleClear = (id) => setJobs(prev => prev.filter(j => j.id !== id));

    const handleSelectFolder = () => {
        Processor.SelectFolder().catch(console.error);
    };

    return (
        <div id="App">
            <div className="header">
                <h1>Manga Optimizer</h1>
                <p>AVIF Smart Compression & Translator Fix</p>
                <div className="header-actions">
                    <button className="btn-add-more" onClick={handleSelectFolder}>+ Añadir carpeta</button>
                </div>
            </div>

            <div
                className={`drop-zone ${(!jobs || jobs.length === 0) ? 'empty' : 'with-jobs'}`}
                onDragOver={(e) => { e.preventDefault(); e.stopPropagation(); }}
                onDrop={(e) => { e.preventDefault(); e.stopPropagation(); }}
            >
                {(!jobs || jobs.length === 0) ? (
                    <div className="drop-zone-content">
                        <div className="icon-big">📁</div>
                        <h2>Arrastra carpetas aquí</h2>
                        <p>o</p>
                        <button className="btn-primary" onClick={handleSelectFolder}>Seleccionar carpeta</button>
                    </div>
                ) : (
                    <div className="jobs-list">
                        {jobs.map(job => (
                            <div key={job.id} className="job-card">
                                <div className="job-info">
                                    <div className="job-path" title={job.sourcePath}>
                                        {job.sourcePath.split(/[\\/]/).pop()}
                                    </div>
                                    <div className={`job-status ${job.status.toLowerCase()}`}>
                                        {job.status}
                                    </div>
                                </div>

                                <div className="progress-container">
                                    <div
                                        className="progress-bar"
                                        style={{ width: `${job.progress}%` }}
                                    ></div>
                                </div>

                                <div className="job-footer">
                                    <div className="job-files">
                                        {job.status === 'completed' ?
                                            `Completado: ${job.totalFiles} imágenes` :
                                            `Procesando: ${job.doneFiles} / ${job.totalFiles}`}
                                    </div>
                                    <div className="controls">
                                        {job.status === 'processing' && (
                                            <button className="btn-small btn-pause" onClick={() => handlePause(job.id)}>Pausar</button>
                                        )}
                                        {job.status === 'paused' && (
                                            <button className="btn-small btn-pause" onClick={() => handleResume(job.id)}>Reanudar</button>
                                        )}
                                        {(job.status === 'processing' || job.status === 'paused') && (
                                            <button className="btn-small btn-stop" onClick={() => handleStop(job.id)}>Parar</button>
                                        )}
                                        {(job.status === 'completed' || job.status === 'stopped' || job.status === 'error') && (
                                            <button className="btn-small btn-clear" onClick={() => handleClear(job.id)}>Eliminar</button>
                                        )}
                                    </div>
                                </div>
                                {job.status === 'processing' && (
                                    <div style={{ fontSize: '0.7rem', color: '#94a3b8', marginTop: '-5px' }}>
                                        Actual: {job.currentFile}
                                    </div>
                                )}
                            </div>
                        ))}
                    </div>
                )}
            </div>
        </div>
    );
}

export default App;
