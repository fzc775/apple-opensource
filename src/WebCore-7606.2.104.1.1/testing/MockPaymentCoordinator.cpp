/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockPaymentCoordinator.h"

#if ENABLE(APPLE_PAY)

#include "MockPayment.h"
#include "MockPaymentContact.h"
#include "MockPaymentMethod.h"
#include "Page.h"
#include "PaymentCoordinator.h"
#include "URL.h"
#include <wtf/RunLoop.h>

namespace WebCore {

MockPaymentCoordinator::MockPaymentCoordinator(Page& page)
    : m_page { page }
{
    m_availablePaymentNetworks.add("amex");
    m_availablePaymentNetworks.add("carteBancaire");
    m_availablePaymentNetworks.add("chinaUnionPay");
    m_availablePaymentNetworks.add("discover");
    m_availablePaymentNetworks.add("interac");
    m_availablePaymentNetworks.add("jcb");
    m_availablePaymentNetworks.add("masterCard");
    m_availablePaymentNetworks.add("privateLabel");
    m_availablePaymentNetworks.add("visa");
}

bool MockPaymentCoordinator::supportsVersion(unsigned version)
{
    ASSERT(version > 0);

#if !ENABLE(APPLE_PAY_SESSION_V3)
    static const unsigned currentVersion = 2;
#elif !ENABLE(APPLE_PAY_SESSION_V4)
    static const unsigned currentVersion = 3;
#else
    static const unsigned currentVersion = 4;
#endif

    return version <= currentVersion;
}

std::optional<String> MockPaymentCoordinator::validatedPaymentNetwork(const String& paymentNetwork)
{
    auto result = m_availablePaymentNetworks.find(paymentNetwork);
    if (result == m_availablePaymentNetworks.end())
        return std::nullopt;
    return *result;
}

bool MockPaymentCoordinator::canMakePayments()
{
    return true;
}

void MockPaymentCoordinator::canMakePaymentsWithActiveCard(const String&, const String&, Function<void(bool)>&& completionHandler)
{
    RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
        completionHandler(true);
    });
}

void MockPaymentCoordinator::openPaymentSetup(const String&, const String&, Function<void(bool)>&& completionHandler)
{
    RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
        completionHandler(true);
    });
}

static uint64_t showCount;
static uint64_t hideCount;

static void dispatchIfShowing(Function<void()>&& function)
{
    ASSERT(showCount > hideCount);
    RunLoop::main().dispatch([currentShowCount = showCount, function = WTFMove(function)]() {
        if (showCount > hideCount && showCount == currentShowCount)
            function();
    });
}

bool MockPaymentCoordinator::showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest& request)
{
    if (request.shippingContact().pkContact())
        m_shippingAddress = request.shippingContact().toApplePayPaymentContact(request.version());

    ASSERT(showCount == hideCount);
    ++showCount;
    dispatchIfShowing([page = &m_page]() {
        page->paymentCoordinator().validateMerchant({ URL(), "https://webkit.org/"_s });
    });
    return true;
}

void MockPaymentCoordinator::completeMerchantValidation(const PaymentMerchantSession&)
{
    dispatchIfShowing([page = &m_page, shippingAddress = m_shippingAddress]() mutable {
        page->paymentCoordinator().didSelectShippingContact(MockPaymentContact { WTFMove(shippingAddress) });
    });
}

static ApplePayLineItem convert(const ApplePaySessionPaymentRequest::LineItem& lineItem)
{
    ApplePayLineItem result;
    result.type = lineItem.type;
    result.label = lineItem.label;
    result.amount = lineItem.amount;
    return result;
}

void MockPaymentCoordinator::updateTotalAndLineItems(const ApplePaySessionPaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    m_total = convert(totalAndLineItems.total);
    m_lineItems.clear();
    for (auto& lineItem : totalAndLineItems.lineItems)
        m_lineItems.append(convert(lineItem));
}

void MockPaymentCoordinator::completeShippingMethodSelection(std::optional<ShippingMethodUpdate>&& shippingMethodUpdate)
{
    if (shippingMethodUpdate)
        updateTotalAndLineItems(shippingMethodUpdate->newTotalAndLineItems);
}

void MockPaymentCoordinator::completeShippingContactSelection(std::optional<ShippingContactUpdate>&& shippingContactUpdate)
{
    if (shippingContactUpdate)
        updateTotalAndLineItems(shippingContactUpdate->newTotalAndLineItems);
}
    
void MockPaymentCoordinator::completePaymentMethodSelection(std::optional<PaymentMethodUpdate>&& paymentMethodUpdate)
{
    if (paymentMethodUpdate)
        updateTotalAndLineItems(paymentMethodUpdate->newTotalAndLineItems);
}

void MockPaymentCoordinator::changeShippingOption(String&& shippingOption)
{
    dispatchIfShowing([page = &m_page, shippingOption = WTFMove(shippingOption)]() mutable {
        ApplePaySessionPaymentRequest::ShippingMethod shippingMethod;
        shippingMethod.identifier = WTFMove(shippingOption);
        page->paymentCoordinator().didSelectShippingMethod(shippingMethod);
    });
}

void MockPaymentCoordinator::changePaymentMethod(ApplePayPaymentMethod&& paymentMethod)
{
    dispatchIfShowing([page = &m_page, paymentMethod = WTFMove(paymentMethod)]() mutable {
        page->paymentCoordinator().didSelectPaymentMethod(MockPaymentMethod { WTFMove(paymentMethod) });
    });
}

void MockPaymentCoordinator::acceptPayment()
{
    dispatchIfShowing([page = &m_page, shippingAddress = m_shippingAddress]() mutable {
        ApplePayPayment payment;
        payment.shippingContact = WTFMove(shippingAddress);
        page->paymentCoordinator().didAuthorizePayment(MockPayment { WTFMove(payment) });
    });
}

void MockPaymentCoordinator::cancelPayment()
{
    dispatchIfShowing([page = &m_page] {
        page->paymentCoordinator().didCancelPaymentSession();
        ++hideCount;
        ASSERT(showCount == hideCount);
    });
}

void MockPaymentCoordinator::completePaymentSession(std::optional<PaymentAuthorizationResult>&&)
{
    ++hideCount;
    ASSERT(showCount == hideCount);
}

void MockPaymentCoordinator::abortPaymentSession()
{
    ++hideCount;
    ASSERT(showCount == hideCount);
}

void MockPaymentCoordinator::cancelPaymentSession()
{
    ++hideCount;
    ASSERT(showCount == hideCount);
}

void MockPaymentCoordinator::paymentCoordinatorDestroyed()
{
    ASSERT(showCount == hideCount);
    delete this;
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)