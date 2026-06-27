function getLatestLastmod(items) {
  let latest;
  for (const item of items) {
    if (!item.lastmod) continue;
    const time = new Date(item.lastmod).getTime();
    if (Number.isNaN(time)) continue;
    if (latest === void 0 || time > latest) {
      latest = time;
    }
  }
  return latest === void 0 ? void 0 : new Date(latest).toISOString();
}
export {
  getLatestLastmod
};
