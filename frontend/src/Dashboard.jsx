import React, { useState } from 'react';
import {
  CheckCircle, XCircle, RefreshCw, Layers, Calendar,
  ChevronDown, ChevronUp, AlertCircle, HelpCircle, Tag,
  ArrowUp, ArrowRight, ArrowDown, Trash2, RotateCcw, Sparkles, AlertTriangle
} from 'lucide-react';

export default function Dashboard({ jobs, onRetryJob, onClearCompleted, onClearAll }) {
  const [expandedJobId, setExpandedJobId] = useState(null);
  const [showFreshStartConfirm, setShowFreshStartConfirm] = useState(false);
  const [isClearing, setIsClearing] = useState(false);

  const totalJobs = jobs.length;
  const runningJobs = jobs.filter(j => j.status === 'RUNNING').length;
  const successJobs = jobs.filter(j => j.status === 'SUCCESS').length;
  const failedJobs = jobs.filter(j => j.status === 'FAILED').length;
  const queuedJobs = jobs.filter(j => j.status === 'QUEUED').length;
  const completedJobsCount = successJobs + failedJobs;

  const toggleExpand = (id) => setExpandedJobId(prev => prev === id ? null : id);

  const handleClearCompletedClick = async () => {
    setIsClearing(true);
    await onClearCompleted();
    setIsClearing(false);
  };

  const getStatusBadge = (status) => {
    const styles = {
      QUEUED:  { cls: 'bg-amber-500/10 text-amber-400 border-amber-500/20',  dot: 'bg-amber-400 animate-pulse' },
      RUNNING: { cls: 'bg-blue-500/10 text-blue-400 border-blue-500/20',     dot: 'bg-blue-400 animate-ping' },
      SUCCESS: { cls: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20', dot: 'bg-emerald-400' },
      FAILED:  { cls: 'bg-rose-500/10 text-rose-400 border-rose-500/20',     dot: 'bg-rose-400' },
    };
    const s = styles[status] || styles.QUEUED;
    return (
      <span className={`inline-flex items-center px-2.5 py-1 text-xs font-semibold rounded-md border ${s.cls}`}>
        <span className={`w-1.5 h-1.5 mr-1.5 rounded-full ${s.dot}`} />
        {status}
      </span>
    );
  };

  const getPriorityBadge = (priority) => {
    const p = {
      HIGH:   { Icon: ArrowUp,    cls: 'text-rose-400 bg-rose-500/10 border-rose-500/20' },
      MEDIUM: { Icon: ArrowRight, cls: 'text-amber-400 bg-amber-500/10 border-amber-500/20' },
      LOW:    { Icon: ArrowDown,  cls: 'text-slate-400 bg-slate-500/10 border-slate-500/20' },
    }[priority] || { Icon: ArrowRight, cls: 'text-slate-400 bg-slate-500/10 border-slate-500/20' };
    return (
      <span className={`inline-flex items-center space-x-1 px-2 py-0.5 text-[10px] font-bold rounded border ${p.cls}`}>
        <p.Icon className="w-2.5 h-2.5" />
        <span>{priority || 'MEDIUM'}</span>
      </span>
    );
  };

  const formatJson = (val) => {
    if (!val) return 'N/A';
    if (typeof val === 'object') return JSON.stringify(val, null, 2);
    return val;
  };

  return (
    <div className="space-y-5">
      {/* Stats */}
      <div className="grid grid-cols-5 gap-3">
        {[
          { label: 'Total',   count: totalJobs,   color: 'text-indigo-400',  border: 'border-indigo-500/10',  bg: 'bg-indigo-500/5'  },
          { label: 'Queued',  count: queuedJobs,  color: 'text-amber-400',   border: 'border-amber-500/10',   bg: 'bg-amber-500/5'   },
          { label: 'Running', count: runningJobs, color: 'text-blue-400',    border: 'border-blue-500/10',    bg: 'bg-blue-500/5'    },
          { label: 'Success', count: successJobs, color: 'text-emerald-400', border: 'border-emerald-500/10', bg: 'bg-emerald-500/5' },
          { label: 'Failed',  count: failedJobs,  color: 'text-rose-400',    border: 'border-rose-500/10',    bg: 'bg-rose-500/5'    },
        ].map((stat, i) => (
          <div key={i} className={`${stat.bg} border ${stat.border} rounded-2xl p-4 flex flex-col`}>
            <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">{stat.label}</span>
            <span className={`text-3xl font-bold ${stat.color} mt-1.5 tracking-tight`}>{stat.count}</span>
          </div>
        ))}
      </div>

      {/* Job Table */}
      <div className="bg-[#0f172a]/60 backdrop-blur-xl border border-slate-800 rounded-2xl shadow-2xl overflow-hidden">
        <div className="px-6 py-4 border-b border-slate-800 flex flex-wrap items-center justify-between gap-3">
          <div className="flex items-center space-x-3">
            <Layers className="w-4.5 h-4.5 text-indigo-400" />
            <h3 className="text-base font-semibold text-white">Task Queue</h3>
            {runningJobs > 0 && (
              <span className="text-xs text-blue-400 font-semibold flex items-center space-x-1.5 bg-blue-500/10 px-2.5 py-1 rounded-full border border-blue-500/20">
                <span className="w-1.5 h-1.5 rounded-full bg-blue-400 animate-ping" />
                <span>{runningJobs} processing</span>
              </span>
            )}
          </div>

          <div className="flex items-center space-x-2">
            {/* Clear Completed Button */}
            <button
              onClick={handleClearCompletedClick}
              disabled={completedJobsCount === 0 || isClearing}
              className="flex items-center space-x-1.5 text-xs font-semibold px-3 py-1.5 rounded-xl bg-slate-800/80 hover:bg-slate-800 border border-slate-700/80 text-slate-300 hover:text-white disabled:opacity-40 disabled:cursor-not-allowed transition-all cursor-pointer shadow-sm"
              title="Clear all finished (SUCCESS and FAILED) jobs"
            >
              <Trash2 className="w-3.5 h-3.5 text-amber-400" />
              <span>Clear Completed</span>
            </button>

            {/* Fresh Start Button */}
            <button
              onClick={() => setShowFreshStartConfirm(true)}
              disabled={totalJobs === 0 || isClearing}
              className="flex items-center space-x-1.5 text-xs font-semibold px-3 py-1.5 rounded-xl bg-indigo-600/20 hover:bg-indigo-600/30 border border-indigo-500/30 text-indigo-300 hover:text-white disabled:opacity-40 disabled:cursor-not-allowed transition-all cursor-pointer shadow-sm"
              title="Reset queue and wipe all jobs for a fresh start"
            >
              <RotateCcw className="w-3.5 h-3.5 text-indigo-400" />
              <span>Fresh Start</span>
            </button>
          </div>
        </div>

        {totalJobs === 0 ? (
          <div className="p-12 text-center flex flex-col items-center">
            <HelpCircle className="w-10 h-10 text-slate-600 mb-3" />
            <p className="text-slate-400 font-medium text-sm">No jobs dispatched yet.</p>
            <p className="text-xs text-slate-500 mt-1">Submit a job from the panel to begin processing.</p>
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-left">
              <thead>
                <tr className="border-b border-slate-800/80 bg-slate-900/30">
                  <th className="px-5 py-3.5 text-[10px] font-bold uppercase tracking-wider text-slate-400 w-12">ID</th>
                  <th className="px-5 py-3.5 text-[10px] font-bold uppercase tracking-wider text-slate-400">Task</th>
                  <th className="px-5 py-3.5 text-[10px] font-bold uppercase tracking-wider text-slate-400 w-28">Status</th>
                  <th className="px-5 py-3.5 text-[10px] font-bold uppercase tracking-wider text-slate-400 w-40">Progress</th>
                  <th className="px-5 py-3.5 text-[10px] font-bold uppercase tracking-wider text-slate-400 w-20">Tries</th>
                  <th className="px-5 py-3.5 text-[10px] font-bold uppercase tracking-wider text-slate-400">Created</th>
                  <th className="px-5 py-3.5 text-[10px] font-bold uppercase tracking-wider text-slate-400 text-right w-20">Actions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-800/40">
                {jobs.map((job) => {
                  const isExpanded = expandedJobId === job.id;
                  const canRetry = job.status === 'FAILED' || job.status === 'SUCCESS';
                  const progress = job.progress ?? 0;
                  const isRunning = job.status === 'RUNNING';
                  return (
                    <React.Fragment key={job.id}>
                      <tr
                        onClick={() => toggleExpand(job.id)}
                        className={`hover:bg-slate-800/20 transition-colors cursor-pointer ${isExpanded ? 'bg-slate-800/10' : ''}`}
                      >
                        <td className="px-5 py-4 text-xs font-bold text-indigo-400">#{job.id}</td>
                        <td className="px-5 py-4">
                          <div className="flex items-start space-x-2">
                            <div>
                              <div className="text-sm font-semibold text-white leading-tight">
                                {job.label || job.type}
                              </div>
                              <div className="flex items-center space-x-2 mt-1">
                                <span className="text-[10px] text-slate-500 font-mono bg-slate-800/60 px-1.5 py-0.5 rounded">
                                  {job.type}
                                </span>
                                {getPriorityBadge(job.priority)}
                              </div>
                            </div>
                          </div>
                        </td>
                        <td className="px-5 py-4">{getStatusBadge(job.status)}</td>
                        <td className="px-5 py-4">
                          <div className="space-y-1">
                            <div className="w-full h-1.5 bg-slate-800 rounded-full overflow-hidden">
                              <div
                                className={`h-full rounded-full transition-all duration-500 ${
                                  job.status === 'SUCCESS' ? 'bg-emerald-500' :
                                  job.status === 'FAILED'  ? 'bg-rose-500' :
                                  isRunning ? 'bg-blue-500' : 'bg-slate-600'
                                } ${isRunning ? 'animate-pulse' : ''}`}
                                style={{ width: `${progress}%` }}
                              />
                            </div>
                            <div className="text-[10px] text-slate-500 font-semibold">{progress}%</div>
                          </div>
                        </td>
                        <td className="px-5 py-4 text-sm font-semibold text-slate-300">{job.attempts}/3</td>
                        <td className="px-5 py-4 text-xs text-slate-400">
                          <div className="flex items-center space-x-1">
                            <Calendar className="w-3 h-3 opacity-50" />
                            <span>{job.created_at}</span>
                          </div>
                        </td>
                        <td className="px-5 py-4" onClick={e => e.stopPropagation()}>
                          <div className="flex items-center justify-end space-x-1">
                            {canRetry && (
                              <button onClick={() => onRetryJob(job.id)}
                                className="p-1.5 hover:bg-indigo-500/15 border border-transparent hover:border-indigo-500/20 rounded-lg text-indigo-400 hover:text-indigo-300 transition-colors cursor-pointer"
                                title="Retry job"
                              >
                                <RefreshCw className="w-3.5 h-3.5" />
                              </button>
                            )}
                            <button onClick={() => toggleExpand(job.id)}
                              className="p-1.5 hover:bg-slate-700/30 rounded-lg text-slate-400 hover:text-slate-200 transition-colors cursor-pointer"
                            >
                              {isExpanded ? <ChevronUp className="w-3.5 h-3.5" /> : <ChevronDown className="w-3.5 h-3.5" />}
                            </button>
                          </div>
                        </td>
                      </tr>

                      {/* Expanded Details */}
                      {isExpanded && (
                        <tr>
                          <td colSpan="7" className="px-5 py-4 bg-[#0a0f1d]/70 border-y border-slate-800/80">
                            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                              <div className="space-y-1.5">
                                <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">Parameters</span>
                                <pre className="p-3.5 bg-slate-950/80 border border-slate-800 rounded-xl text-xs font-mono text-indigo-300 overflow-x-auto max-h-44 whitespace-pre-wrap">
                                  {formatJson(job.payload)}
                                </pre>
                              </div>
                              <div className="space-y-1.5">
                                <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">
                                  {job.status === 'FAILED' ? 'Error Trace' : 'Execution Output'}
                                </span>
                                {job.status === 'FAILED' ? (
                                  <div className="p-3.5 bg-rose-950/20 border border-rose-800/30 rounded-xl flex items-start space-x-2 text-xs font-mono text-rose-300 overflow-x-auto max-h-44">
                                    <AlertCircle className="w-4 h-4 text-rose-400 shrink-0 mt-0.5" />
                                    <span className="whitespace-pre-wrap">{job.result || 'No error recorded.'}</span>
                                  </div>
                                ) : (
                                  <pre className="p-3.5 bg-slate-950/80 border border-slate-800 rounded-xl text-xs font-mono text-emerald-400 overflow-x-auto max-h-44 whitespace-pre-wrap">
                                    {formatJson(job.result) || 'Awaiting execution…'}
                                  </pre>
                                )}
                              </div>
                            </div>
                          </td>
                        </tr>
                      )}
                    </React.Fragment>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Fresh Start Modal Confirmation */}
      {showFreshStartConfirm && (
        <div className="fixed inset-0 z-50 bg-black/70 backdrop-blur-sm flex items-center justify-center p-4">
          <div className="bg-[#0f172a] border border-slate-800 rounded-2xl p-6 max-w-md w-full shadow-2xl space-y-4">
            <div className="flex items-center space-x-3 text-amber-400">
              <div className="p-2.5 bg-amber-500/10 border border-amber-500/20 rounded-xl">
                <AlertTriangle className="w-6 h-6 text-amber-400" />
              </div>
              <div>
                <h4 className="text-base font-bold text-white">Confirm Fresh Start</h4>
                <p className="text-xs text-slate-400">Wipe task queue and start clean</p>
              </div>
            </div>

            <p className="text-xs text-slate-300 leading-relaxed">
              This will permanently delete all <strong className="text-white">{totalJobs} jobs</strong> from the database, purge pending jobs in Redis, and reset the task ID sequence back to #1.
            </p>

            <div className="flex items-center justify-end space-x-3 pt-2">
              <button
                onClick={() => setShowFreshStartConfirm(false)}
                className="px-4 py-2 text-xs font-semibold text-slate-400 hover:text-white bg-slate-800/60 rounded-xl hover:bg-slate-800 border border-slate-700/60 transition-colors cursor-pointer"
              >
                Cancel
              </button>
              <button
                onClick={async () => {
                  setShowFreshStartConfirm(false);
                  setIsClearing(true);
                  await onClearAll();
                  setIsClearing(false);
                }}
                className="px-4 py-2 text-xs font-semibold text-white bg-indigo-600 hover:bg-indigo-500 rounded-xl shadow-lg shadow-indigo-600/30 transition-all cursor-pointer flex items-center space-x-1.5"
              >
                <Sparkles className="w-3.5 h-3.5" />
                <span>Perform Fresh Start</span>
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
