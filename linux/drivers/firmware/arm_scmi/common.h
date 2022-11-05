/*
 * System Control and Management Interface (SCMI) Message Protocol
 * driver common header file containing some definitions, structures
 * and function prototypes used in all the different SCMI protocols.
 *
 * Copyright (C) 2017 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/scmi_protocol.h>
#include <linux/types.h>

#define PROTOCOL_REV_MINOR_BITS	16
#define PROTOCOL_REV_MINOR_MASK	((1U << PROTOCOL_REV_MINOR_BITS) - 1)
#define PROTOCOL_REV_MAJOR(x)	((x) >> PROTOCOL_REV_MINOR_BITS)
#define PROTOCOL_REV_MINOR(x)	((x) & PROTOCOL_REV_MINOR_MASK)
#define MAX_PROTOCOLS_IMP	16
#define MAX_OPPS		16

enum scmi_common_cmd {
	PROTOCOL_VERSION = 0x0,
	PROTOCOL_ATTRIBUTES = 0x1,
	PROTOCOL_MESSAGE_ATTRIBUTES = 0x2,
};

/**
 * struct scmi_msg_resp_prot_version - Response for a message
 *
 * @major_version: Major version of the ABI that firmware supports
 * @minor_version: Minor version of the ABI that firmware supports
 *
 * In general, ABI version changes follow the rule that minor version increments
 * are backward compatible. Major revision changes in ABI may not be
 * backward compatible.
 *
 * Response to a generic message with message type SCMI_MSG_VERSION
 */
struct scmi_msg_resp_prot_version {
	__le16 minor_version;
	__le16 major_version;
};

/**
 * struct scmi_msg_hdr - Message(Tx/Rx) header
 *
 * @id: The identifier of the command being sent
 * @protocol_id: The identifier of the protocol used to send @id command
 * @seq: The token to identify the message. when a message/command returns,
 *       the platform returns the whole message header unmodified including
 *	 the token.
 */
struct scmi_msg_hdr {
	u8 id;
	u8 protocol_id;
	u16 seq;
	u32 status;
	bool poll_completion;
};

/**
 * struct scmi_msg - Message(Tx/Rx) structure
 *
 * @buf: Buffer pointer
 * @len: Length of data in the Buffer
 */
struct scmi_msg {
	void *buf;
	size_t len;
};

/**
 * struct scmi_xfer - Structure representing a message flow
 *
 * @hdr: Transmit message header
 * @tx: Transmit message
 * @rx: Receive message, the buffer should be pre-allocated to store
 *	message. If request-ACK protocol is used, we can reuse the same
 *	buffer for the rx path as we use for the tx path.
 * @done: completion event
 */

struct scmi_xfer {
	void *con_priv;
	struct scmi_msg_hdr hdr;
	struct scmi_msg tx;
	struct scmi_msg rx;
	struct completion done;
};

void scmi_one_xfer_put(const struct scmi_handle *h, struct scmi_xfer *xfer);
int scmi_do_xfer(const struct scmi_handle *h, struct scmi_xfer *xfer);
int scmi_one_xfer_init(const struct scmi_handle *h, u8 msg_id, u8 prot_id,
		       size_t tx_size, size_t rx_size, struct scmi_xfer **p);
int scmi_handle_put(const struct scmi_handle *handle);
struct scmi_handle *scmi_handle_get(struct device *dev);
void scmi_set_handle(struct scmi_device *scmi_dev);
int scmi_version_get(const struct scmi_handle *h, u8 protocol, u32 *version);
void scmi_setup_protocol_implemented(const struct scmi_handle *handle,
				     u8 *prot_imp);

int scmi_base_protocol_init(struct scmi_handle *h);
