export interface SignalPathNodeConfigUpdateDirtyFields {
  dirty?: boolean;
  persist?: boolean;
}

export function shouldMarkSignalPathNodeConfigUpdateDirty(update: SignalPathNodeConfigUpdateDirtyFields): boolean {
  return update.dirty === true || (update.dirty !== false && update.persist !== false);
}