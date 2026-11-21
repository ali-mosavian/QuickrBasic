// A file uploaded to the page: its path in the file system, an 8.3 name, and whether the IDE opens it. A
// native module (.QBX) is kept beside the programs for an IMPORT to find.
export function uploadAs(name) {
  const short = name.toUpperCase().replace(/[^A-Z0-9_.]/g, '').slice(0, 12);
  return { path: '/' + short, open: !short.endsWith('.QBX') };
}
