import { observable, computed, action } from "mobx";

export default class Monitor {
    @observable hasActiveNotification = false;
    @observable items = [];

    lastUpdateTimestamp = 0;
    refreshTimer = null;

    startRefresh() {
        this.clearRefreshTimer();
        this.refreshTimer = setInterval(() => {
            if (Date.now() - this.lastUpdateTimestamp > 6000) {
                this.setHasActiveNotification(false);
                this.clearRefreshTimer();
            }
        }, 500);
    }

    clearRefreshTimer() {
        if (this.refreshTimer !== null) {
            clearInterval(this.refreshTimer);
            this.refreshTimer = null;
        }
    }

    @action setHasActiveNotification(value) {
        this.hasActiveNotification = value;
    }

    @action update(world) {
        if (!world.monitor && !world.notification) {
            return;
        }

        let newItems = [];
        if (world.notification) {
            newItems = world.notification.reverse().map(notification => {
                return Object.assign(notification.item, {
                    timestampMs: notification.timestampSec * 1000,
                });
            });
        } else if (world.monitor) {
            // deprecated: no timestamp for each item
            newItems = world.monitor.item;
        }

        if (this.hasNewNotification(this.items, newItems)) {
            this.hasActiveNotification = true;
            this.lastUpdateTimestamp = Date.now();
            this.items.replace(newItems);
            this.startRefresh();
        }
    }

    hasNewNotification(items, newItems) {
        if (items.length === 0 && newItems.length === 0) {
            return false;
        }
        if (items.length === 0 || newItems.length === 0) {
            return true;
        }
        return JSON.stringify(this.items[0]) !== JSON.stringify(newItems[0]);
    }

    // Inserts the provided message into the items. This is for
    // debug purpose only.
    @action insert(level, message, timestamp) {
        const newItems = [];
        newItems.push({
            msg: message,
            logLevel: level
        });

        for (let i = 0; i < this.items.length; ++i) {
            if (i < 29) {
                newItems.push(this.items[i]);
            }
        }

        this.hasActiveNotification = true;
        this.lastUpdateTimestamp = timestamp;
        this.items.replace(newItems);
        this.startRefresh();
    }
}
