import React, { useState } from 'react';
import { Send, AlertTriangle, Cpu, Sliders, Zap, RotateCcw } from 'lucide-react';
import { API_BASE } from './config.js';

const BUILT_IN_JOBS = [
  { type: 'IMAGE_RESIZE',   label: 'Image Resize',     duration: '~2s',  desc: 'Resize & compress images to target resolution',        fields: [{ name: 'width', label: 'Width px', default: '1280' }, { name: 'height', label: 'Height px', default: '720' }] },
  { type: 'PDF_GENERATE',   label: 'PDF Generate',     duration: '~3s',  desc: 'Compile HTML template into a PDF document',            fields: [{ name: 'template', label: 'Template ID', default: 'invoice-v2' }] },
  { type: 'EMAIL_SEND',     label: 'Email Send',       duration: '~1s',  desc: 'Dispatch transactional email via SMTP relay',           fields: [{ name: 'to', label: 'Recipient', default: 'user@example.com' }] },
  { type: 'DATA_EXPORT',    label: 'Data Export',      duration: '~4s',  desc: 'Export database records to CSV / Parquet file',        fields: [{ name: 'format', label: 'Format', default: 'CSV' }] },
  { type: 'VIDEO_TRANSCODE',label: 'Video Transcode',  duration: '~6s',  desc: 'Re-encode video stream to H.264 / WebM codec',        fields: [{ name: 'resolution', label: 'Target Resolution', default: '1080p' }] },
  { type: 'DB_BACKUP',      label: 'Database Backup',  duration: '~5s',  desc: 'Snapshot all tables into a compressed SQL archive',    fields: [] },
  { type: 'CUSTOM',         label: '✦ Custom Job',      duration: 'custom', desc: 'Define your own task name, duration, and output',  fields: [] },
];

export default function SubmitJob({ onJobSubmitted }) {
  const [selectedType, setSelectedType] = useState('IMAGE_RESIZE');
  const [priority, setPriority] = useState('MEDIUM');
  const [failType, setFailType] = useState('NONE');
  const [failAttempts, setFailAttempts] = useState(2);
  // Custom job fields
  const [customLabel, setCustomLabel] = useState('');
  const [customDuration, setCustomDuration] = useState(3);
  const [customOutput, setCustomOutput] = useState('');
  // Built-in job extra fields
  const [extraFields, setExtraFields] = useState({});
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');

  const selected = BUILT_IN_JOBS.find(j => j.type === selectedType);
  const isCustom = selectedType === 'CUSTOM';

  const handleSubmit = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError('');
    setSuccess('');

    let payloadObj = {};

    if (isCustom) {
      if (!customLabel.trim()) { setError('Custom job requires a name.'); setLoading(false); return; }
      payloadObj.duration_ms = customDuration * 1000;
      if (customOutput.trim()) payloadObj.output = customOutput.trim();
    } else {
      // Merge extra built-in field values
      selected.fields.forEach(f => {
        const val = extraFields[f.name] || f.default;
        if (val) payloadObj[f.name] = val;
      });
    }

    if (failType === 'PERMANENT') payloadObj.fail = true;
    else if (failType === 'TEMPORARY') payloadObj.fail_attempts = parseInt(failAttempts) || 1;

    const label = isCustom ? customLabel.trim() : selected.label;

    try {
      const response = await fetch(`${API_BASE}/api/jobs`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          type: selectedType,
          label,
          priority,
          payload: Object.keys(payloadObj).length > 0 ? payloadObj : null,
        }),
      });
      if (!response.ok) {
        const errData = await response.json();
        throw new Error(errData.error || 'Server error');
      }
      setSuccess(`Job "${label}" dispatched!`);
      setExtraFields({});
      if (isCustom) { setCustomLabel(''); setCustomOutput(''); setCustomDuration(3); }
      setTimeout(() => setSuccess(''), 2500);
      if (onJobSubmitted) onJobSubmitted();
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="bg-[#0f172a]/60 backdrop-blur-xl border border-slate-800 rounded-2xl p-6 shadow-2xl shadow-indigo-500/5">
      <div className="flex items-center space-x-3 mb-5">
        <div className="p-2.5 bg-indigo-500/10 rounded-xl border border-indigo-500/20">
          <Cpu className="w-5 h-5 text-indigo-400" />
        </div>
        <h2 className="text-xl font-semibold tracking-tight text-white">Dispatch Task</h2>
      </div>

      <form onSubmit={handleSubmit} className="space-y-5">
        {/* Job Type Grid */}
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-slate-400 mb-2">Job Type</label>
          <div className="grid grid-cols-2 gap-2">
            {BUILT_IN_JOBS.map(job => (
              <button
                key={job.type}
                type="button"
                onClick={() => { setSelectedType(job.type); setExtraFields({}); }}
                className={`text-left p-3 rounded-xl border transition-all ${
                  selectedType === job.type
                    ? 'bg-indigo-600/20 border-indigo-500/50 text-white'
                    : 'bg-slate-800/30 border-slate-700/50 text-slate-400 hover:border-slate-600'
                }`}
              >
                <div className="text-xs font-semibold">{job.label}</div>
                <div className="text-[10px] opacity-60 mt-0.5">{job.duration}</div>
              </button>
            ))}
          </div>
          {selected && (
            <p className="text-[11px] text-slate-500 mt-2 px-1">{selected.desc}</p>
          )}
        </div>

        {/* Custom Job Builder */}
        {isCustom && (
          <div className="space-y-3 p-4 bg-indigo-500/5 border border-indigo-500/15 rounded-xl">
            <div className="flex items-center space-x-2 text-xs font-semibold text-indigo-400 mb-1">
              <Sliders className="w-3.5 h-3.5" />
              <span>Custom Job Configuration</span>
            </div>
            <div>
              <label className="block text-xs text-slate-400 mb-1">Task Name *</label>
              <input
                type="text"
                value={customLabel}
                onChange={e => setCustomLabel(e.target.value)}
                placeholder="e.g. Sync Shopify Orders"
                className="w-full bg-[#1e293b]/60 border border-slate-700 rounded-lg px-3 py-2.5 text-sm text-white placeholder-slate-500 focus:outline-none focus:border-indigo-500 transition-colors"
              />
            </div>
            <div>
              <label className="block text-xs text-slate-400 mb-1">
                Simulated Duration: <span className="text-indigo-400 font-semibold">{customDuration}s</span>
              </label>
              <input
                type="range" min="1" max="15" value={customDuration}
                onChange={e => setCustomDuration(parseInt(e.target.value))}
                className="w-full accent-indigo-500 h-1.5 bg-slate-800 rounded-lg cursor-pointer"
              />
              <div className="flex justify-between text-[10px] text-slate-600 mt-1">
                <span>1s</span><span>15s</span>
              </div>
            </div>
            <div>
              <label className="block text-xs text-slate-400 mb-1">Expected Output (optional)</label>
              <input
                type="text"
                value={customOutput}
                onChange={e => setCustomOutput(e.target.value)}
                placeholder="e.g. /exports/shopify_sync.json"
                className="w-full bg-[#1e293b]/60 border border-slate-700 rounded-lg px-3 py-2.5 text-sm text-white placeholder-slate-500 focus:outline-none focus:border-indigo-500 transition-colors"
              />
            </div>
          </div>
        )}

        {/* Built-in extra fields */}
        {!isCustom && selected?.fields.length > 0 && (
          <div className="space-y-3">
            {selected.fields.map(f => (
              <div key={f.name}>
                <label className="block text-xs text-slate-400 mb-1">{f.label}</label>
                <input
                  type="text"
                  value={extraFields[f.name] ?? ''}
                  onChange={e => setExtraFields(prev => ({ ...prev, [f.name]: e.target.value }))}
                  placeholder={f.default}
                  className="w-full bg-[#1e293b]/60 border border-slate-700 rounded-lg px-3 py-2.5 text-sm text-white placeholder-slate-500 focus:outline-none focus:border-indigo-500 transition-colors"
                />
              </div>
            ))}
          </div>
        )}

        {/* Priority */}
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-slate-400 mb-2">
            Priority
          </label>
          <div className="grid grid-cols-3 gap-2 p-1 bg-[#1e293b]/40 border border-slate-800 rounded-xl">
            {[
              { id: 'HIGH', label: '↑ High', color: 'text-rose-400' },
              { id: 'MEDIUM', label: '→ Medium', color: 'text-amber-400' },
              { id: 'LOW', label: '↓ Low', color: 'text-slate-400' },
            ].map(p => (
              <button key={p.id} type="button"
                onClick={() => setPriority(p.id)}
                className={`py-2 text-xs font-semibold rounded-lg transition-all ${
                  priority === p.id
                    ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20'
                    : `${p.color} hover:text-slate-200`
                }`}
              >
                {p.label}
              </button>
            ))}
          </div>
        </div>

        {/* Fault Injection */}
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-slate-400 mb-2">
            Fault Injection
          </label>
          <div className="grid grid-cols-3 gap-2 p-1 bg-[#1e293b]/40 border border-slate-800 rounded-xl">
            {[
              { id: 'NONE', label: 'None' },
              { id: 'TEMPORARY', label: 'Retry N×' },
              { id: 'PERMANENT', label: 'Hard Fail' },
            ].map(opt => (
              <button key={opt.id} type="button" onClick={() => setFailType(opt.id)}
                className={`py-2 text-xs font-medium rounded-lg transition-all ${
                  failType === opt.id
                    ? 'bg-rose-600/80 text-white shadow-lg shadow-rose-600/10'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                {opt.label}
              </button>
            ))}
          </div>

          {failType === 'TEMPORARY' && (
            <div className="mt-3 space-y-1">
              <div className="flex items-center space-x-3">
                <input type="range" min="1" max="3" value={failAttempts}
                  onChange={e => setFailAttempts(parseInt(e.target.value))}
                  className="flex-1 accent-rose-500 h-1.5 bg-slate-800 rounded-lg cursor-pointer"
                />
                <span className="text-sm font-bold text-rose-400 bg-rose-500/10 px-2.5 py-1 border border-rose-500/20 rounded-md">
                  {failAttempts}×
                </span>
              </div>
              <p className="text-[10px] text-slate-500">Fails first {failAttempts} attempts, then succeeds.</p>
            </div>
          )}

          {failType === 'PERMANENT' && (
            <div className="mt-2 p-2.5 bg-rose-500/10 border border-rose-500/20 rounded-xl flex items-center space-x-2 text-xs text-rose-300">
              <AlertTriangle className="w-4 h-4 shrink-0" />
              <span>All 3 attempts fail → permanently FAILED.</span>
            </div>
          )}
        </div>

        {error && (
          <div className="p-3 bg-rose-500/10 border border-rose-500/20 rounded-xl text-xs text-rose-400 font-medium">{error}</div>
        )}
        {success && (
          <div className="p-3 bg-emerald-500/10 border border-emerald-500/20 rounded-xl text-xs text-emerald-400 font-semibold flex items-center space-x-2">
            <Zap className="w-3.5 h-3.5" />
            <span>{success}</span>
          </div>
        )}

        <button type="submit" disabled={loading}
          className="w-full flex items-center justify-center space-x-2 bg-indigo-600 hover:bg-indigo-500 active:bg-indigo-700 disabled:bg-slate-800 disabled:text-slate-600 text-white font-semibold py-3.5 rounded-xl shadow-lg shadow-indigo-600/20 hover:shadow-indigo-500/30 transition-all cursor-pointer"
        >
          {loading ? (
            <span className="w-5 h-5 border-2 border-white/30 border-t-white rounded-full animate-spin" />
          ) : (
            <><Send className="w-4 h-4" /><span>Dispatch Job</span></>
          )}
        </button>
      </form>
    </div>
  );
}
